#pragma once

#include "CoreMinimal.h"
#include "SHMAIProvider.h"
#include "Interfaces/IHttpRequest.h"

// ============================================================================
// LLM Provider —— OpenAI 兼容 HTTP 端点
//
// **它是可失败的，而且失败被视为正常路径**：超时、非 200、畸形 JSON、
// 模型拒答，一律回调 bSuccess=false，由 DirectorCore 降级到本地表。
// 玩家对此零感知——这正是"断网完整可玩"这个卖点的实现方式。
//
// 配置（全部走环境变量，**key 绝不入库、绝不进日志**）：
//   SHM_LLM_API_KEY   必填。为空时 IsAvailable()=false，DirectorCore 直接选本地
//   SHM_LLM_BASE_URL  选填，默认 https://api.openai.com/v1
//                     ——做成可配置是刻意的：将来把它指向自建中转服务，
//                       本类一行代码都不用改
//   SHM_LLM_MODEL     选填，默认 gpt-4o-mini
// ============================================================================
class SHANHAIMIRROR_API FSHMLlmProvider : public ISHMAIProvider
{
public:
	FSHMLlmProvider();

	virtual void RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone) override;
	virtual FString GetProviderName() const override { return TEXT("Llm"); }

	// 是否可用。key 为空、或已因鉴权失败自我停用时返回 false
	bool IsAvailable() const { return !ApiKey.IsEmpty() && !bAuthFailed; }

	const FString& GetBaseUrl() const { return BaseUrl; }
	const FString& GetModel()   const { return Model; }
	float GetTimeoutSeconds()   const { return TimeoutSeconds; }

	// 默认超时 10 秒。**这个值是实测定的，不是拍的**：
	// DeepSeek(deepseek-chat) 实测单次往返 3.8~5.0s+，原先设 5s 会频繁误判超时、
	// 白白降级。可用 SHM_LLM_TIMEOUT 覆盖——换更快的端点时应调小。
	// 注意与 USHMFloorManager::MaxDecisionWaitSeconds 的关系：那个必须比这个大，
	// 否则玩法层会先于 LLM 放弃，等待就成了空耗。
	static constexpr float DefaultTimeoutSeconds = 10.f;

private:
	void HandleResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedOk,
	                    FSHMOnIntentReady OnDone, double StartTime);

	// 把请求/响应原文落盘（脱敏）——回放脚本、前端样例、"LLM 怎么翻车的"真实案例，
	// 事后都无法补造。见 Saved/SHMLlmLogs/（已 gitignore）
	void ArchiveExchange(const FString& RequestBody, const FString& ResponseBody, int32 HttpCode) const;

	FString ApiKey;    // 只在内存，日志里只出现「已配置/未配置」
	FString BaseUrl;
	FString Model;
	float   TimeoutSeconds = DefaultTimeoutSeconds;

	// 鉴权失败（401/403）后自我停用：**key 是错的，重试多少次都是错的**。
	// 不停用的话，一个废 key 会让每一层都白等一次超时/白发一次请求，
	// 整局体验被拖慢却毫无收益。
	bool bAuthFailed = false;
};
