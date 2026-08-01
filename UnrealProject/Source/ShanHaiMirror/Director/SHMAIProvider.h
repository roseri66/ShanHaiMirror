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
// 四个实现（DECISIONS D-16 定了前三个，D-23 加了 Remote）：
//   FSHMLocalProvider   规则表决策 —— 永远可用，降级终点
//   FSHMRemoteProvider  决策网关后端 HTTP —— 可失败（D-23）
//   FSHMLlmProvider     OpenAI 兼容 HTTP 直连 —— 可失败，默认不编译（见其头文件）
//   FSHMReplayProvider  预录 JSON 脚本 —— 确定性回放，录屏/集成测试
// ============================================================================
class SHANHAIMIRROR_API ISHMAIProvider
{
public:
	virtual ~ISHMAIProvider() = default;

	virtual void RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone) = 0;

	// 日志/溯源用标识（进 FDirectorTrace.ProviderId）
	virtual FString GetProviderName() const = 0;

	// 一局开始时通知 Provider（D-23）。
	//
	// **默认空实现**，所以现有三个 Provider 一行都不用改。
	// 只有 Remote 需要它：上行请求体要带 runId，而 runId 是"一局的标识"，
	// 不是"一次决策的输入"，故不在 FDirectorContext 里 —— 把它塞进 Context
	// 会让那个结构体承担它不该承担的语义，也会污染四个已经稳定的护栏测试。
	//
	// 用 DirectorCore 的 RunId 而非自己生成：请求体里的 runId 必须与决策日志
	// 的 runId 对得上，否则 M5 的回流聚合无法把两边关联起来。
	virtual void OnRunStarted(const FString& InRunId) {}
};
