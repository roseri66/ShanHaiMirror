#include "SHMEncounterMath.h"

namespace
{
	// TMap 迭代顺序不保证——排序后建桶，同输入才有同输出
	TArray<TPair<FGameplayTag, float>> SortedPositiveWeights(const TMap<FGameplayTag, float>& Weights)
	{
		TArray<TPair<FGameplayTag, float>> Sorted;
		for (const TPair<FGameplayTag, float>& Pair : Weights)
		{
			if (Pair.Value > 0.f && Pair.Key.IsValid())
			{
				Sorted.Add(Pair);
			}
		}
		Sorted.Sort([](const TPair<FGameplayTag, float>& A, const TPair<FGameplayTag, float>& B)
		{
			return A.Key.GetTagName().LexicalLess(B.Key.GetTagName());
		});
		return Sorted;
	}
}

FGameplayTag FSHMEncounterMath::PickArchetype(const TMap<FGameplayTag, float>& Weights, float Rand01)
{
	const TArray<TPair<FGameplayTag, float>> Sorted = SortedPositiveWeights(Weights);
	if (Sorted.Num() == 0)
	{
		return FGameplayTag();   // 空/全零权重：无效，调用方自行处理
	}

	float Total = 0.f;
	for (const TPair<FGameplayTag, float>& Pair : Sorted)
	{
		Total += Pair.Value;
	}

	// 权重不要求归一化（护栏管的是决策合法性，这里对任意正权重都稳健）
	const float Target = FMath::Clamp(Rand01, 0.f, 1.f) * Total;

	float Cursor = 0.f;
	for (const TPair<FGameplayTag, float>& Pair : Sorted)
	{
		Cursor += Pair.Value;
		if (Target < Cursor)
		{
			return Pair.Key;
		}
	}

	// Rand01 = 1.0 时浮点边界会走到这里：钳到最后一桶
	return Sorted.Last().Key;
}

TArray<FGameplayTag> FSHMEncounterMath::BuildWave(const TMap<FGameplayTag, float>& Weights,
	const TMap<FGameplayTag, int32>& ThreatValues, int32 ThreatBudget, FRandomStream& Rand)
{
	TArray<FGameplayTag> Wave;

	// 只保留有威胁值的候选，并记下最便宜的——预算连它都装不下时收工
	TMap<FGameplayTag, float> Candidates;
	int32 MinThreat = MAX_int32;
	for (const TPair<FGameplayTag, float>& Pair : Weights)
	{
		if (const int32* Threat = ThreatValues.Find(Pair.Key))
		{
			if (*Threat > 0 && Pair.Value > 0.f)
			{
				Candidates.Add(Pair.Key, Pair.Value);
				MinThreat = FMath::Min(MinThreat, *Threat);
			}
		}
	}
	if (Candidates.Num() == 0)
	{
		return Wave;
	}

	int32 Remaining = ThreatBudget;
	int32 Guard = 100;   // 防御：权重/预算异常时不死循环
	while (Remaining >= MinThreat && Guard-- > 0)
	{
		const FGameplayTag Picked = PickArchetype(Candidates, Rand.FRand());
		if (!Picked.IsValid())
		{
			break;
		}

		const int32 Threat = ThreatValues[Picked];
		if (Threat > Remaining)
		{
			continue;   // 这只装不下，重抽（Remaining ≥ MinThreat 保证总会抽到能装下的）
		}

		Wave.Add(Picked);
		Remaining -= Threat;
	}

	return Wave;
}
