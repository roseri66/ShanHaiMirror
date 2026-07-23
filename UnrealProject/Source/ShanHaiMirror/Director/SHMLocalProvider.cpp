#include "SHMLocalProvider.h"
#include "Framework/SHMGameplayTags.h"

namespace
{
	// 决策阈值（Provider 内部策略参数，不是护栏——护栏的在 Validator 里）
	constexpr float HighPressureThreshold  = 60.f;   // 生存压力高于此进入恢复期
	constexpr float CounterConcentration   = 85.f;   // 集中度高于此才定向反制
	constexpr float HighEfficiency         = 70.f;   // 效率高于此施压
	constexpr float MinConfidenceToAct     = 0.6f;   // 低于此只允许轻度、最多 1 条

	// 从候选集挑一条 (标签, 偏好等级) 的规则；偏好等级不可用/超预算则降级到 light
	const FSHMAvailableRule* PickRule(const FDirectorContext& Ctx, FGameplayTag Tag,
		const FString& PreferredLevel, int32 RemainingBudget,
		const TArray<FRuleIntent>& AlreadyPicked)
	{
		auto FindRule = [&Ctx](FGameplayTag InTag, const FString& InLevel) -> const FSHMAvailableRule*
		{
			return Ctx.AvailableRules.FindByPredicate([&](const FSHMAvailableRule& R)
			{
				return R.RuleTag == InTag && R.Level == InLevel;
			});
		};

		// 与已选规则互斥的不取——Provider 主动避冲突，而不是靠护栏打回重来
		auto ConflictsWithPicked = [&](const FSHMAvailableRule* Candidate)
		{
			for (const FRuleIntent& Picked : AlreadyPicked)
			{
				if (Candidate->ConflictsWith.HasTag(Picked.RuleTag)) { return true; }
				if (const FSHMAvailableRule* PickedAvail = FindRule(Picked.RuleTag, Picked.Level))
				{
					if (PickedAvail->ConflictsWith.HasTag(Candidate->RuleTag)) { return true; }
				}
			}
			return false;
		};

		for (const FString& Level : { PreferredLevel, FString(TEXT("light")) })
		{
			if (const FSHMAvailableRule* Found = FindRule(Tag, Level))
			{
				if (Found->Cost <= RemainingBudget && !ConflictsWithPicked(Found))
				{
					return Found;
				}
			}
		}
		return nullptr;
	}
}

FDirectorIntent FSHMLocalProvider::RequestIntent(const FDirectorContext& Ctx)
{
	const FPlayerProfile& P = Ctx.Profile;
	FDirectorIntent Intent;

	const bool bRanged = P.PrimaryBuildTags.Contains(SHMTags::Build_Ranged.GetTag());
	const bool bMelee  = P.PrimaryBuildTags.Contains(SHMTags::Build_Melee.GetTag());

	// ------------------------------------------------------------------
	// ① 挑战等级（优先级从高到低：先保玩家体验，再谈针对）
	// ------------------------------------------------------------------
	if (P.SurvivalPressure >= HighPressureThreshold)
	{
		Intent.ChallengeLevel = EChallengeLevel::Recovery;
	}
	else if (P.Confidence >= MinConfidenceToAct && P.BuildConcentration > CounterConcentration && (bRanged || bMelee))
	{
		Intent.ChallengeLevel = EChallengeLevel::Counter;
	}
	else if (P.CombatEfficiency >= HighEfficiency)
	{
		Intent.ChallengeLevel = EChallengeLevel::Pressure;
	}
	else
	{
		Intent.ChallengeLevel = EChallengeLevel::Stable;
	}

	// ------------------------------------------------------------------
	// ② 敌人原型配比（先给原始权重，再按候选集过滤并归一化）
	// ------------------------------------------------------------------
	TMap<FGameplayTag, float> Raw;
	switch (Intent.ChallengeLevel)
	{
	case EChallengeLevel::Recovery:
		// GDD §6.2「挣扎中」：只出基础杂兵，给喘息空间
		Raw.Add(SHMTags::Enemy_Grunt.GetTag(), 1.0f);
		break;

	case EChallengeLevel::Counter:
		if (bRanged)
		{
			// 反制远程：Tank 压缩输出空间 + Rush 打断站桩
			Raw.Add(SHMTags::Enemy_Grunt.GetTag(),   0.40f);
			Raw.Add(SHMTags::Enemy_Tank.GetTag(),    0.30f);
			Raw.Add(SHMTags::Enemy_Rush.GetTag(),    0.20f);
			Raw.Add(SHMTags::Enemy_Shooter.GetTag(), 0.10f);
		}
		else // bMelee
		{
			// 反制近战：Shooter 拉开距离消耗
			Raw.Add(SHMTags::Enemy_Grunt.GetTag(),   0.45f);
			Raw.Add(SHMTags::Enemy_Shooter.GetTag(), 0.35f);
			Raw.Add(SHMTags::Enemy_Tank.GetTag(),    0.10f);
			Raw.Add(SHMTags::Enemy_Rush.GetTag(),    0.10f);
		}
		break;

	default: // Stable / Pressure：均衡配比
		Raw.Add(SHMTags::Enemy_Grunt.GetTag(),   0.55f);
		Raw.Add(SHMTags::Enemy_Tank.GetTag(),    0.15f);
		Raw.Add(SHMTags::Enemy_Rush.GetTag(),    0.15f);
		Raw.Add(SHMTags::Enemy_Shooter.GetTag(), 0.15f);
		break;
	}

	// 过滤到候选原型并归一化（候选集缩水时权重和仍保证为 1，Schema 护栏要求）
	float Sum = 0.f;
	for (const TPair<FGameplayTag, float>& Pair : Raw)
	{
		if (Ctx.AvailableArchetypes.Contains(Pair.Key))
		{
			Intent.EnemyWeights.Add(Pair.Key, Pair.Value);
			Sum += Pair.Value;
		}
	}
	if (Sum > 0.f)
	{
		for (TPair<FGameplayTag, float>& Pair : Intent.EnemyWeights)
		{
			Pair.Value /= Sum;
		}
	}

	// ------------------------------------------------------------------
	// ③ 规则选择（恢复期零规则；低置信度只轻度、最多 1 条；预算内避冲突）
	// ------------------------------------------------------------------
	if (Intent.ChallengeLevel != EChallengeLevel::Recovery)
	{
		const bool bLowConfidence = P.Confidence < MinConfidenceToAct;
		const int32 MaxRules      = bLowConfidence ? 1 : 2;
		const FString Preferred   = bLowConfidence ? TEXT("light") : TEXT("medium");

		// 按画像方向排规则偏好（顺序即优先级）
		TArray<FGameplayTag> Preference;
		if (bRanged)
		{
			Preference = { FGameplayTag::RequestGameplayTag("Rule.Ammo"),
			               FGameplayTag::RequestGameplayTag("Rule.Cooldown") };
		}
		else if (bMelee)
		{
			Preference = { FGameplayTag::RequestGameplayTag("Rule.MeleeDamage"),
			               FGameplayTag::RequestGameplayTag("Rule.Cooldown") };
		}
		else
		{
			Preference = { FGameplayTag::RequestGameplayTag("Rule.Cooldown"),
			               FGameplayTag::RequestGameplayTag("Rule.Heal") };
		}

		int32 RemainingBudget = Ctx.ChallengeBudget;
		for (const FGameplayTag& Tag : Preference)
		{
			if (Intent.RuleIntents.Num() >= MaxRules) { break; }

			if (const FSHMAvailableRule* Picked = PickRule(Ctx, Tag, Preferred, RemainingBudget, Intent.RuleIntents))
			{
				FRuleIntent RI;
				RI.RuleTag = Picked->RuleTag;
				RI.Level   = Picked->Level;
				Intent.RuleIntents.Add(RI);
				RemainingBudget -= Picked->Cost;
			}
		}
	}

	// ------------------------------------------------------------------
	// ④ 台词与理由（模板句；W4 起由 LLM 生成个性化版本）
	// ------------------------------------------------------------------
	switch (Intent.ChallengeLevel)
	{
	case EChallengeLevel::Recovery:
		Intent.Narration = TEXT("这一层，我为你留了喘息之地。");
		Intent.Reason    = TEXT("生存压力过高，降压：全杂兵、零规则。");
		break;
	case EChallengeLevel::Counter:
		Intent.Narration = bRanged
			? TEXT("你的弓用得很好。但这一层，别指望站在原地。")
			: TEXT("刀锋所及皆是坦途？这一层试试够不到的敌人。");
		Intent.Reason = FString::Printf(TEXT("高置信度(%.2f)定向反制 %s 打法。"),
			P.Confidence, bRanged ? TEXT("远程") : TEXT("近战"));
		break;
	case EChallengeLevel::Pressure:
		Intent.Narration = TEXT("你走得太顺了。镜中的试炼，该加深了。");
		Intent.Reason    = TEXT("战斗效率偏高，整体施压。");
		break;
	default:
		Intent.Narration = TEXT("继续吧。我在看着。");
		Intent.Reason    = TEXT("画像尚不明确，维持均衡配比。");
		break;
	}

	return Intent;
}
