#include "SHMRemoteProvider.h"
#include "Framework/SHMDebugCheats.h"
#include "SHMDirectorWireFormat.h"
#include "SHMJsonIntent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHMRemote, Log, All);

const TCHAR* FSHMRemoteProvider::IntentPath = TEXT("v1/director/intent");

namespace
{
	// 服务端回传的 meta 走响应头，不进 body ——
	// 这样 body 与 LLM 原始输出、与 Data/ReplayScripts/*.json 三者字节级同格式，
	// 任何一次真实响应都能直接另存为回放脚本。
	const TCHAR* HeaderSource  = TEXT("X-SHM-Source");
	const TCHAR* HeaderCache   = TEXT("X-SHM-Cache");
	const TCHAR* HeaderElapsed = TEXT("X-SHM-Elapsed-Ms");
	const TCHAR* HeaderDebug   = TEXT("X-SHM-Debug");
}

FSHMRemoteProvider::FSHMRemoteProvider()
{
	const FString Configured = FPlatformMisc::GetEnvironmentVariable(TEXT("SHM_DIRECTOR_URL"));

	// 空 = 用默认；显式 off/disabled = 关掉本 Provider。
	// 两者语义不同：Spring 那边的 ${VAR:} 空值兜底与"未配置"也是两种语义，
	// 别混（踩坑 #20 的服务端版）。
	if (Configured.Equals(TEXT("off"), ESearchCase::IgnoreCase) ||
		Configured.Equals(TEXT("disabled"), ESearchCase::IgnoreCase))
	{
		BaseUrl.Empty();
	}
	else
	{
		BaseUrl = Configured.IsEmpty() ? TEXT("http://localhost:8080") : Configured;
	}

	const FString TimeoutStr = FPlatformMisc::GetEnvironmentVariable(TEXT("SHM_DIRECTOR_TIMEOUT"));
	if (!TimeoutStr.IsEmpty())
	{
		TimeoutSeconds = FMath::Max(1.f, FCString::Atof(*TimeoutStr));
	}

	UE_LOG(LogSHMRemote, Log, TEXT("Remote Provider 初始化：endpoint=%s timeout=%.0fs"),
		BaseUrl.IsEmpty() ? TEXT("（已禁用）") : *BaseUrl, TimeoutSeconds);

	if (IsAvailable())
	{
		ProbeReachability();
	}
}

void FSHMRemoteProvider::ProbeReachability()
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Probe = FHttpModule::Get().CreateRequest();
	Probe->SetURL(BaseUrl);
	Probe->SetVerb(TEXT("GET"));
	// 短超时：这只是开局的一次提示性探测，不该让人等。
	// 2 秒足够判断本机服务在不在。
	Probe->SetTimeout(2.f);

	const TWeakPtr<uint8> WeakToken = LifetimeToken;
	const FString Url = BaseUrl;
	Probe->OnProcessRequestComplete().BindLambda(
		[WeakToken, Url](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
	{
		if (!WeakToken.IsValid())
		{
			return;
		}

		// **拿到任何 HTTP 响应都算可达**——根路径返回 404 是正常的
		// （服务端没有映射 "/"），那同样证明服务活着。
		// 只有传输层失败才说明连不上。
		if (bOk && Resp.IsValid())
		{
			UE_LOG(LogSHMRemote, Log, TEXT("决策网关可达（%s，HTTP %d），本局将使用远端决策。"),
				*Url, Resp->GetResponseCode());
			return;
		}

		UE_LOG(LogSHMRemote, Warning,
			TEXT("╔══════════════════════════════════════════════════════════════╗\n")
			TEXT("║ 决策网关不可达：%s\n")
			TEXT("║ **本局将全程使用本地 Provider**（游戏完整可玩，但无 LLM 决策）。\n")
			TEXT("║ 若要启用远端决策，先启动服务再开始游戏：\n")
			TEXT("║     cd C:\\Dev\\ShanHaiMirror\\DirectorService\n")
			TEXT("║     mvn spring-boot:run\n")
			TEXT("╚══════════════════════════════════════════════════════════════╝"),
			*Url);
	});

	Probe->ProcessRequest();
}

FSHMRemoteProvider::FSHMRemoteProvider(const FTestingConfig& Config)
	: BaseUrl(Config.BaseUrl)
	, TimeoutSeconds(Config.TimeoutSeconds)
{
	// 刻意不读环境变量、不打日志：测试构造只做赋值。
}

void FSHMRemoteProvider::RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone)
{
	if (!IsAvailable())
	{
		UE_LOG(LogSHMRemote, Warning, TEXT("Remote Provider 已禁用，本次判失败（将降级本地）"));
		LastFailureReason = TEXT("已禁用");
		OnDone.ExecuteIfBound(FDirectorIntent(), false);
		return;
	}
	LastFailureReason.Empty();

	// 请求体走上行契约的唯一真源，不在这里手拼（见 SHMDirectorWireFormat.h）
	const FString Body = FSHMDirectorWire::BuildIntentRequestString(Context, RunId);
	const double  StartTime = FPlatformTime::Seconds();

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl / IntentPath);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// D-25：开着调试作弊时把倍率带给服务端，落进 debug_flags 列。
	// ⚠️ 这一句不是可选的日志，是**数据可信度的边界**：作弊会抬高战斗效率的速度分，
	//    这批数据能答「去掉某字段会不会合并指纹」，不能答「真实命中率是多少」。
	//    靠人记得住哪批是作弊数据是不可能的 —— 所以让它跟着请求一起落库。
	if (FSHMDebugCheats::IsAnyActive())
	{
		Request->SetHeader(HeaderDebug, FSHMDebugCheats::DescribeActive());
	}

	Request->SetContentAsString(Body);
	Request->SetTimeout(TimeoutSeconds);

	// **WeakToken 必须捕获**：见头文件里 LifetimeToken 的说明 ——
	// 玩家在 HTTP 往返途中停掉 PIE 时，本对象已析构而回调仍会执行。
	const TWeakPtr<uint8> WeakToken = LifetimeToken;
	Request->OnProcessRequestComplete().BindLambda(
		[this, WeakToken, OnDone, StartTime](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
	{
		// Provider 已随 GameInstance 析构：一个字段都不能碰。
		// 也不需要回调——等结果的 DirectorCore 同样已经不在了。
		// **令牌 pin 住之后 this 才是安全的**，下面写 LastFailureReason 依赖这一点。
		if (!WeakToken.IsValid())
		{
			return;
		}

		const float ElapsedMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);

		// --- 降级② 传输层失败（后端没起 / 超时 / 断网 / DNS）---
		// 「后端进程停掉但网络正常」是本次新增的失败模式：
		// 以前"有网"就意味着"能到 LLM"，现在中间多了一跳。
		if (!bOk || !Resp.IsValid())
		{
			// 区分"没起"与"起了但太慢"：两者的处置完全不同——
			// 前者是去启动服务，后者是查上游或调超时。
			// 界面上分不清的话，第一反应都会是"是不是代码坏了"。
			const bool bLikelyTimeout = ElapsedMs >= TimeoutSeconds * 1000.f * 0.9f;
			LastFailureReason = bLikelyTimeout ? TEXT("超时") : TEXT("不可达");

			UE_LOG(LogSHMRemote, Warning,
				TEXT("请求失败（%s），耗时 %.0fms —— 将降级本地%s"),
				*LastFailureReason, ElapsedMs,
				bLikelyTimeout ? TEXT("") : TEXT("。服务没启动？先跑 mvn spring-boot:run"));
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		const int32   Code     = Resp->GetResponseCode();
		const FString RespBody = Resp->GetContentAsString();

		if (Code != 200)
		{
			// 429 不是错误，是**设计内的降级路径**：服务端限流保护上游 LLM 配额，
			// 客户端照常降本地，玩家零感知。单独记一条日志是为了让
			// "被限流了" 与 "后端挂了" 在事后可区分。
			if (Code == 429)
			{
				LastFailureReason = TEXT("被限流");
				UE_LOG(LogSHMRemote, Log,
					TEXT("HTTP 429 被限流（设计内），耗时 %.0fms —— 按预期降级本地"), ElapsedMs);
			}
			else
			{
				LastFailureReason = FString::Printf(TEXT("HTTP %d"), Code);
				UE_LOG(LogSHMRemote, Warning,
					TEXT("HTTP %d，耗时 %.0fms —— 将降级本地"), Code, ElapsedMs);
			}
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		// --- 降级③ 200 但 body 不是合法 Intent ---
		// **不剥信封**：响应体就是 Intent 本体，直接交给共用解析器。
		// FSHMJsonIntent 是"不信任 LLM 输出的第一道关卡"，
		// 在这里原封不动地变成"不信任后端输出"——数值字段照样丢弃，
		// 幻觉标签照样解析出来交护栏拒绝。自己的服务也不给免检特权。
		bool bParsed = false;
		const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(RespBody, bParsed);
		if (!bParsed)
		{
			LastFailureReason = TEXT("响应无法解析");
			UE_LOG(LogSHMRemote, Warning,
				TEXT("后端返回 200 但 body 不是合法 Intent，耗时 %.0fms —— 将降级本地"), ElapsedMs);
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		// meta 在响应头里。落日志是为了让"这次是缓存命中还是真调了 LLM"事后可查——
		// 不记的话，缓存命中率调得好不好就只能靠猜（设计文档 §6 的分桶参数
		// 本来就是拍的，要靠这些数据回头校准）。
		const FString Source = Resp->GetHeader(HeaderSource);
		const FString Cache  = Resp->GetHeader(HeaderCache);
		UE_LOG(LogSHMRemote, Log,
			TEXT("决策成功：来源=%s 缓存=%s 客户端耗时=%.0fms 服务端自报=%sms"),
			Source.IsEmpty() ? TEXT("未标注") : *Source,
			Cache.IsEmpty()  ? TEXT("未标注") : *Cache,
			ElapsedMs,
			*Resp->GetHeader(HeaderElapsed));

		OnDone.ExecuteIfBound(Intent, true);
	});

	UE_LOG(LogSHMRemote, Log, TEXT("向 %s 发起决策请求（第 %d 层，runId=%s）"),
		*(BaseUrl / IntentPath), Context.FloorIndex,
		RunId.IsEmpty() ? TEXT("未设置") : *RunId);

	Request->ProcessRequest();
}
