#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

// ============================================================================
// Provider 接口 —— AI Director 链路第 ④ 步 CHOOSE
//
// 职责：在 Context 给定的候选集内做选择，产出 FDirectorIntent。
// **明确不做**：不校验（第 ⑤ 步）、不接触数值（第 ⑥ 步）、不改游戏对象。
//
// 三个实现（DECISIONS D-16）：
//   FSHMLocalProvider   规则表决策 —— 永远可用，降级终点（本文件同目录，W2）
//   FSHMLlmProvider     OpenAI 兼容 HTTP —— 可失败（W4）
//   FSHMReplayProvider  预录 JSON 脚本 —— 确定性回放，录屏/集成测试（W4）
//
// 纯 C++ 接口而非 UInterface：Provider 无需暴露给蓝图/反射，
// DirectorCore 以 TUniquePtr 持有，可在运行时无痛替换。
// ============================================================================
class SHANHAIMIRROR_API ISHMAIProvider
{
public:
	virtual ~ISHMAIProvider() = default;

	virtual FDirectorIntent RequestIntent(const FDirectorContext& Context) = 0;

	// 日志/调试用标识
	virtual FString GetProviderName() const = 0;
};
