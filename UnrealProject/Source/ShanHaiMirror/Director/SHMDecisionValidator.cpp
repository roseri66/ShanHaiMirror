#include "SHMDecisionValidator.h"

namespace
{
	void Reject(FValidationResult& InOut, FString Reason)
	{
		InOut.bValid = false;
		InOut.RejectReasons.Add(MoveTemp(Reason));
	}

	bool IsLegalLevel(const FString& Level)
	{
		return Level == TEXT("light") || Level == TEXT("medium") || Level == TEXT("heavy");
	}

	// 在候选集中找 (标签, 等级) 完全匹配的规则
	const FSHMAvailableRule* FindAvailable(const FDirectorContext& Context, const FRuleIntent& Intent)
	{
		return Context.AvailableRules.FindByPredicate([&](const FSHMAvailableRule& R)
		{
			return R.RuleTag == Intent.RuleTag && R.Level == Intent.Level;
		});
	}
}

FValidationResult FSHMDecisionValidator::Validate(const FDirectorIntent& Intent, const FDirectorContext& Context)
{
	// 不在第一条违规就短路——把全部问题一次列出，排查 Provider（尤其是 LLM）时省一半来回
	FValidationResult Result;
	CheckSchema  (Intent, Context, Result);
	CheckBudget  (Intent, Context, Result);
	CheckConflict(Intent, Context, Result);
	CheckFairness(Intent, Context, Result);
	return Result;
}

// ============================================================================
//  ① Schema：结构合法性——权重和为 1、标签全在白名单内、等级字符串合法
// ============================================================================
void FSHMDecisionValidator::CheckSchema(const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut)
{
	float WeightSum = 0.f;
	for (const TPair<FGameplayTag, float>& Pair : Intent.EnemyWeights)
	{
		if (!Context.AvailableArchetypes.Contains(Pair.Key))
		{
			Reject(InOut, FString::Printf(TEXT("[Schema] 原型 %s 不在白名单"), *Pair.Key.ToString()));
		}
		if (Pair.Value < 0.f || Pair.Value > 1.f)
		{
			Reject(InOut, FString::Printf(TEXT("[Schema] 原型 %s 权重 %.2f 越界"), *Pair.Key.ToString(), Pair.Value));
		}
		WeightSum += Pair.Value;
	}

	if (!FMath::IsNearlyEqual(WeightSum, 1.f, WeightSumTolerance))
	{
		Reject(InOut, FString::Printf(TEXT("[Schema] 权重和 %.3f ≠ 1"), WeightSum));
	}

	for (const FRuleIntent& Rule : Intent.RuleIntents)
	{
		if (!IsLegalLevel(Rule.Level))
		{
			Reject(InOut, FString::Printf(TEXT("[Schema] 非法等级 '%s'"), *Rule.Level));
		}
		else if (!FindAvailable(Context, Rule))
		{
			Reject(InOut, FString::Printf(TEXT("[Schema] 规则 %s/%s 不在候选集"),
				*Rule.RuleTag.ToString(), *Rule.Level));
		}
	}
}

// ============================================================================
//  ② Budget：Σcost ≤ 本层挑战预算
// ============================================================================
void FSHMDecisionValidator::CheckBudget(const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut)
{
	int32 TotalCost = 0;
	for (const FRuleIntent& Rule : Intent.RuleIntents)
	{
		if (const FSHMAvailableRule* Avail = FindAvailable(Context, Rule))
		{
			TotalCost += Avail->Cost;
		}
		// 候选集里找不到的规则由 Schema 拒绝，这里不重复计费
	}

	if (TotalCost > Context.ChallengeBudget)
	{
		Reject(InOut, FString::Printf(TEXT("[Budget] Σcost %d 超出预算 %d"),
			TotalCost, Context.ChallengeBudget));
	}
}

// ============================================================================
//  ③ Conflict：互斥规则不同时出现（如 弹药↓ + 远程伤害↓ = 逼玩家入无解）
// ============================================================================
void FSHMDecisionValidator::CheckConflict(const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut)
{
	for (int32 i = 0; i < Intent.RuleIntents.Num(); ++i)
	{
		const FSHMAvailableRule* AvailI = FindAvailable(Context, Intent.RuleIntents[i]);
		if (!AvailI) { continue; }

		for (int32 j = i + 1; j < Intent.RuleIntents.Num(); ++j)
		{
			const FSHMAvailableRule* AvailJ = FindAvailable(Context, Intent.RuleIntents[j]);
			if (!AvailJ) { continue; }

			// 双向检查——数据表理应对称填写，但护栏不赌数据填对
			const bool bClash =
				AvailI->ConflictsWith.HasTag(AvailJ->RuleTag) ||
				AvailJ->ConflictsWith.HasTag(AvailI->RuleTag);

			if (bClash)
			{
				Reject(InOut, FString::Printf(TEXT("[Conflict] %s 与 %s 互斥"),
					*AvailI->RuleTag.ToString(), *AvailJ->RuleTag.ToString()));
			}
		}
	}
}

// ============================================================================
//  ④ Fairness：同一规则不连续第 3 层；低置信度禁重度
// ============================================================================
void FSHMDecisionValidator::CheckFairness(const FDirectorIntent& Intent, const FDirectorContext& Context, FValidationResult& InOut)
{
	// --- 连续性：某规则已在最近 MaxConsecutiveFloors 层每层都出现，本层再出现即拒 ---
	if (Context.DecisionHistory.Num() >= MaxConsecutiveFloors)
	{
		for (const FRuleIntent& Rule : Intent.RuleIntents)
		{
			bool bInAllRecent = true;
			for (int32 k = 1; k <= MaxConsecutiveFloors; ++k)
			{
				const FDirectorHistoryEntry& Entry = Context.DecisionHistory[Context.DecisionHistory.Num() - k];
				if (!Entry.AppliedRuleTags.Contains(Rule.RuleTag))
				{
					bInAllRecent = false;
					break;
				}
			}

			if (bInAllRecent)
			{
				Reject(InOut, FString::Printf(TEXT("[Fairness] %s 已连续 %d 层，不得连续第 %d 层"),
					*Rule.RuleTag.ToString(), MaxConsecutiveFloors, MaxConsecutiveFloors + 1));
			}
		}
	}

	// --- 置信度：玩家可能刚换打法，低置信度时禁止重度针对 ---
	if (Context.Profile.Confidence < MinConfidenceForHeavy)
	{
		for (const FRuleIntent& Rule : Intent.RuleIntents)
		{
			if (Rule.Level == TEXT("heavy"))
			{
				Reject(InOut, FString::Printf(TEXT("[Fairness] 置信度 %.2f < %.2f，禁止重度规则 %s"),
					Context.Profile.Confidence, MinConfidenceForHeavy, *Rule.RuleTag.ToString()));
			}
		}
	}
}
