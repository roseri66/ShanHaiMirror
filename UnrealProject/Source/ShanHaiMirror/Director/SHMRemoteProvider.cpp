#include "SHMRemoteProvider.h"
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
		OnDone.ExecuteIfBound(FDirectorIntent(), false);
		return;
	}

	// 请求体走上行契约的唯一真源，不在这里手拼（见 SHMDirectorWireFormat.h）
	const FString Body = FSHMDirectorWire::BuildIntentRequestString(Context, RunId);
	const double  StartTime = FPlatformTime::Seconds();

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl / IntentPath);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(Body);
	Request->SetTimeout(TimeoutSeconds);

	// **WeakToken 必须捕获**：见头文件里 LifetimeToken 的说明 ——
	// 玩家在 HTTP 往返途中停掉 PIE 时，本对象已析构而回调仍会执行。
	const TWeakPtr<uint8> WeakToken = LifetimeToken;
	Request->OnProcessRequestComplete().BindLambda(
		[WeakToken, OnDone, StartTime](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
	{
		// Provider 已随 GameInstance 析构：一个字段都不能碰。
		// 也不需要回调——等结果的 DirectorCore 同样已经不在了。
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
			UE_LOG(LogSHMRemote, Warning,
				TEXT("请求失败（后端不可达或超时），耗时 %.0fms —— 将降级本地"), ElapsedMs);
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
				UE_LOG(LogSHMRemote, Log,
					TEXT("HTTP 429 被限流（设计内），耗时 %.0fms —— 按预期降级本地"), ElapsedMs);
			}
			else
			{
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
