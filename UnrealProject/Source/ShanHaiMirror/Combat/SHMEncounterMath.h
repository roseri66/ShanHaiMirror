#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

// ============================================================================
// 遭遇数学 —— 按导演权重抽原型、按威胁预算组波次
//
// 纯函数：随机数由调用方传入（Rand01 / FRandomStream），同输入必同输出，可单测。
// 权重桶按标签名排序后累计——TMap 迭代顺序不保证，不排序结果就不可复现。
// ============================================================================
class SHANHAIMIRROR_API FSHMEncounterMath
{
public:
	// 权重分布中抽一个原型。Rand01 ∈ [0,1)。空权重/全零权重返回无效标签。
	static FGameplayTag PickArchetype(const TMap<FGameplayTag, float>& Weights, float Rand01);

	// 按威胁预算组一波：反复抽原型直到预算装不下任何候选。
	// ThreatValues 缺失的原型视为不可抽。保证 Σ威胁 ≤ ThreatBudget。
	static TArray<FGameplayTag> BuildWave(const TMap<FGameplayTag, float>& Weights,
		const TMap<FGameplayTag, int32>& ThreatValues, int32 ThreatBudget, FRandomStream& Rand);
};
