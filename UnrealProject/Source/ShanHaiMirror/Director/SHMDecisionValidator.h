#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

// ============================================================================
// 决策校验器 —— AI Director 链路第 ⑤ 步 VALIDATE，四道护栏
//
// (Intent, Context) 的纯函数：Cost/冲突信息在构建 Context 时已从 DT_Rule 拷入
// FSHMAvailableRule，校验时不碰数据表、不碰世界——可单测。
//
// 存在意义：W4 的 LLM Provider 会产出**不可信输出**（非法标签、超预算组合、
// 连续针对同一弱点）。护栏现在就建好并测死，LLM 接入时安全网已就位。
// 本地 Provider 的输出理论上总能通过——若不通过，那是 Provider 的 bug，
// 护栏的日志会把它暴露出来。
//
// 这是 DECISIONS §4.7 红线项：四道护栏 + 各自的拒绝路径测试，任何情况不砍。
// ============================================================================
class SHANHAIMIRROR_API FSHMDecisionValidator
{
public:
	// 跑全部四道护栏，收集所有违规（不在第一条就短路，便于一次看全问题）
	static FValidationResult Validate(const FDirectorIntent& Intent, const FDirectorContext& Context);

	// --- 四道护栏（公开供测试单独调用，精确定位失败点）---
	static void CheckSchema  (const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut);
	static void CheckBudget  (const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut);
	static void CheckConflict(const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut);
	static void CheckFairness(const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut);

	// --- 常量（测试引用，避免魔法数字漂移）---
	static constexpr float WeightSumTolerance     = 0.01f;  // 权重和允许的浮点误差
	static constexpr float MinConfidenceForHeavy  = 0.6f;   // 低于此置信度禁重度规则
	static constexpr int32 MaxConsecutiveFloors   = 2;      // 同一规则最多连续层数（第 3 层拒绝）
};
