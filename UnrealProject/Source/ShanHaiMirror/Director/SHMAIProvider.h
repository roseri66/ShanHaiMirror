#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

// 决策意图就绪回调。bSuccess=false 表示本 Provider 这次交不出可用结果，
// 调用方必须降级（超时/HTTP 错/解析失败/脚本缺这一层，都走这里）。
DECLARE_DELEGATE_TwoParams(FSHMOnIntentReady, const FDirectorIntent& /*Intent*/, bool /*bSuccess*/);

// ============================================================================
// Provider 接口 —— AI Director 链路第 ④ 步 CHOOSE
//
// 职责：在 Context 给定的候选集内做选择，产出 FDirectorIntent。
// **明确不做**：不校验（第 ⑤ 步）、不接触数值（第 ⑥ 步）、不改游戏对象。
//
// **为什么是异步**：LLM 是 1-2 秒的 HTTP 往返，同步接口会卡住游戏线程。
// 本地/回放 Provider 在调用内立即回调（同步完成），LLM Provider 在响应返回时回调——
// 调用方一律按异步写，不需要知道背后是哪种。
// 延迟被层间 2.5 秒过场天然掩盖（TDD §1.2 早就设想的结构，这里落地）。
//
// 三个实现（DECISIONS D-16）：
//   FSHMLocalProvider   规则表决策 —— 永远可用，降级终点
//   FSHMLlmProvider     OpenAI 兼容 HTTP —— 可失败
//   FSHMReplayProvider  预录 JSON 脚本 —— 确定性回放，录屏/集成测试
// ============================================================================
class SHANHAIMIRROR_API ISHMAIProvider
{
public:
	virtual ~ISHMAIProvider() = default;

	virtual void RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone) = 0;

	// 日志/溯源用标识（进 FDirectorTrace.ProviderId）
	virtual FString GetProviderName() const = 0;
};
