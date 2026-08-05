#include "SHMLlmProvider.h"
#include "SHMPromptBuilder.h"
#include "SHMJsonIntent.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

#if SHM_DEV_DIRECT_LLM   // 直连模式，默认不编译（D-23）

DEFINE_LOG_CATEGORY_STATIC(LogSHMLlm, Log, All);

namespace
{
	FString EnvOrDefault(const TCHAR* Name, const FString& Default)
	{
		const FString Value = FPlatformMisc::GetEnvironmentVariable(Name);
		return Value.IsEmpty() ? Default : Value;
	}
}

FSHMLlmProvider::FSHMLlmProvider()
{
	ApiKey  = FPlatformMisc::GetEnvironmentVariable(TEXT("SHM_LLM_API_KEY"));
	BaseUrl = EnvOrDefault(TEXT("SHM_LLM_BASE_URL"), TEXT("https://api.openai.com/v1"));
	Model   = EnvOrDefault(TEXT("SHM_LLM_MODEL"),    TEXT("gpt-4o-mini"));

	const FString TimeoutStr = FPlatformMisc::GetEnvironmentVariable(TEXT("SHM_LLM_TIMEOUT"));
	if (!TimeoutStr.IsEmpty())
	{
		TimeoutSeconds = FMath::Max(1.f, FCString::Atof(*TimeoutStr));
	}

	// 只报告「有没有」，绝不打印 key 本身
	UE_LOG(LogSHMLlm, Log, TEXT("LLM Provider 初始化：endpoint=%s model=%s timeout=%.0fs key=%s"),
		*BaseUrl, *Model, TimeoutSeconds, ApiKey.IsEmpty() ? TEXT("未配置") : TEXT("已配置"));
}

void FSHMLlmProvider::RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone)
{
	if (!IsAvailable())
	{
		UE_LOG(LogSHMLlm, Warning, TEXT("SHM_LLM_API_KEY 未配置，本次请求直接判失败（将降级本地）"));
		OnDone.ExecuteIfBound(FDirectorIntent(), false);
		return;
	}

	const FString Body = FSHMPromptBuilder::BuildRequestBody(Context, Model);
	const double  StartTime = FPlatformTime::Seconds();

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BaseUrl / TEXT("chat/completions"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetContentAsString(Body);
	Request->SetTimeout(TimeoutSeconds);

	// 捕获 Body 供落档；OnDone 按值捕获，保证回调时仍有效。
	// **WeakToken 必须捕获**：见头文件里 LifetimeToken 的说明 ——
	// 玩家在 HTTP 往返途中停掉 PIE 时，本对象已析构而回调仍会执行。
	const TWeakPtr<uint8> WeakToken = LifetimeToken;
	Request->OnProcessRequestComplete().BindLambda(
		[this, WeakToken, OnDone, StartTime, Body](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
	{
		// Provider 已随 GameInstance 析构：this 是野指针，一个字段都不能碰。
		// 也不需要回调——等结果的 DirectorCore 同样已经不在了。
		if (!WeakToken.IsValid())
		{
			return;
		}

		const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;
		const FString RespBody = Resp.IsValid() ? Resp->GetContentAsString() : FString();
		ArchiveExchange(Body, RespBody, Code);

		const float ElapsedMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);

		// --- 传输层失败（超时/断网/DNS）---
		if (!bOk || !Resp.IsValid())
		{
			UE_LOG(LogSHMLlm, Warning, TEXT("请求失败（超时或无网络），耗时 %.0fms —— 将降级本地"), ElapsedMs);
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		// --- HTTP 层失败（401/429/500…）---
		if (Code != 200)
		{
			// 鉴权错：key 本身是错的，重试多少次都一样。自我停用，
			// 避免后面每一层都白发一次请求（一个废 key 能拖慢整局）
			if (Code == 401 || Code == 403)
			{
				bAuthFailed = true;
				UE_LOG(LogSHMLlm, Error,
					TEXT("HTTP %d 鉴权失败——SHM_LLM_API_KEY 无效。**本局不再尝试 LLM**，")
					TEXT("全部由本地规则表决策。请检查环境变量后重启编辑器。"), Code);
			}
			else
			{
				UE_LOG(LogSHMLlm, Warning, TEXT("HTTP %d，耗时 %.0fms —— 将降级本地"), Code, ElapsedMs);
			}
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		// --- 剥 OpenAI 信封：choices[0].message.content 才是模型输出 ---
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RespBody);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogSHMLlm, Warning, TEXT("响应不是合法 JSON —— 将降级本地"));
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (!Root->TryGetArrayField(TEXT("choices"), Choices) || !Choices || Choices->Num() == 0)
		{
			UE_LOG(LogSHMLlm, Warning, TEXT("响应无 choices —— 将降级本地"));
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		const TSharedPtr<FJsonObject>* FirstChoice = nullptr;
		const TSharedPtr<FJsonObject>* Message = nullptr;
		FString Content;
		if (!(*Choices)[0]->TryGetObject(FirstChoice) || !FirstChoice ||
			!(*FirstChoice)->TryGetObjectField(TEXT("message"), Message) || !Message ||
			!(*Message)->TryGetStringField(TEXT("content"), Content))
		{
			UE_LOG(LogSHMLlm, Warning, TEXT("响应结构不符预期 —— 将降级本地"));
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		// 模型有时会用 markdown 代码围栏包 JSON，即便 prompt 明令禁止。
		// 这种"几乎对了"的情况值得容错——比白白降级一次划算。
		Content = Content.TrimStartAndEnd();
		if (Content.StartsWith(TEXT("```")))
		{
			int32 FirstNewline = INDEX_NONE;
			Content.FindChar(TEXT('\n'), FirstNewline);
			if (FirstNewline != INDEX_NONE)
			{
				Content = Content.Mid(FirstNewline + 1);
			}
			const int32 LastFence = Content.Find(TEXT("```"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (LastFence != INDEX_NONE)
			{
				Content = Content.Left(LastFence);
			}
			Content = Content.TrimStartAndEnd();
		}

		// --- 交给与回放共用的解析器（不信任 LLM 输出的第一道关卡）---
		bool bParsed = false;
		const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(Content, bParsed);

		if (!bParsed)
		{
			UE_LOG(LogSHMLlm, Warning, TEXT("模型输出无法解析为可用 Intent —— 将降级本地"));
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
			return;
		}

		UE_LOG(LogSHMLlm, Log, TEXT("决策就绪，耗时 %.0fms：%s"), ElapsedMs, *Intent.Narration);
		OnDone.ExecuteIfBound(Intent, true);
	});

	Request->ProcessRequest();
}

void FSHMLlmProvider::ArchiveExchange(const FString& RequestBody, const FString& ResponseBody, int32 HttpCode) const
{
	// 真实样本事后无法补造：回放脚本、前端调试数据、"LLM 如何翻车"的案例都靠它。
	// 请求体里不含 key（key 在 header），所以落盘天然脱敏。
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("SHMLlmLogs");
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_%s"));

	FString Archive;
	Archive += FString::Printf(TEXT("=== HTTP %d @ %s ===\n"), HttpCode, *Stamp);
	Archive += TEXT("--- REQUEST ---\n") + RequestBody + TEXT("\n");
	Archive += TEXT("--- RESPONSE ---\n") + ResponseBody + TEXT("\n");

	// UTF-8：留档要能被 grep / python / 前端直接读（同决策日志，见 ExportDecisionLog）
	FFileHelper::SaveStringToFile(Archive, *(Dir / FString::Printf(TEXT("%s.txt"), *Stamp)),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

#endif // SHM_DEV_DIRECT_LLM
