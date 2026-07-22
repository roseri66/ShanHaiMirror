#include "SHMProfileAnalyzer.h"
#include "Framework/SHMGameplayTags.h"

// ============================================================================
//  Build 集中度
//  = 最大单武器攻击次数 / 总攻击次数 × 100
// ============================================================================
float FSHMProfileAnalyzer::ComputeBuildConcentration(const FFloorBehaviorSnapshot& Snapshot)
{
	int32 Total = 0;
	int32 Max   = 0;

	for (const TPair<FGameplayTag, int32>& Pair : Snapshot.WeaponAttackCounts)
	{
		Total += Pair.Value;
		Max = FMath::Max(Max, Pair.Value);
	}

	// 零攻击：玩家可能一次没打就走到出口。这里是全链路最容易炸的除零点。
	if (Total <= 0)
	{
		return 0.f;
	}

	return FMath::Clamp(static_cast<float>(Max) / static_cast<float>(Total) * 100.f, 0.f, 100.f);
}

// ============================================================================
//  武器 → Build 流派
//  MVP 硬编码。后续改为读 DT_Weapon.BuildTags——届时映射表由调用方传入，
//  本函数保持纯粹（不在这里读 DataTable，那会毁掉可测性）。
// ============================================================================
FGameplayTag FSHMProfileAnalyzer::GetBuildTagForWeapon(const FGameplayTag& WeaponTag)
{
	if (WeaponTag == SHMTags::Weapon_Bow.GetTag())
	{
		return SHMTags::Build_Ranged.GetTag();
	}
	if (WeaponTag == SHMTags::Weapon_Sword.GetTag())
	{
		return SHMTags::Build_Melee.GetTag();
	}
	return FGameplayTag();
}

// ============================================================================
//  主力打法标签
//  只有集中度超过阈值才产出——这是"AI 不针对没有明显偏好的玩家"的工程落点
// ============================================================================
TArray<FGameplayTag> FSHMProfileAnalyzer::ComputePrimaryBuildTags(const FFloorBehaviorSnapshot& Snapshot)
{
	TArray<FGameplayTag> Result;

	if (ComputeBuildConcentration(Snapshot) <= PrimaryBuildThreshold)
	{
		return Result;
	}

	// 找出攻击次数最多的那把武器
	FGameplayTag TopWeapon;
	int32 TopCount = 0;
	for (const TPair<FGameplayTag, int32>& Pair : Snapshot.WeaponAttackCounts)
	{
		if (Pair.Value > TopCount)
		{
			TopCount  = Pair.Value;
			TopWeapon = Pair.Key;
		}
	}

	const FGameplayTag BuildTag = GetBuildTagForWeapon(TopWeapon);
	if (BuildTag.IsValid())
	{
		Result.Add(BuildTag);
	}

	return Result;
}

// ============================================================================
//  单层主导原型
// ============================================================================
FGameplayTag FSHMProfileAnalyzer::ComputeDominantArchetype(const FFloorBehaviorSnapshot& Snapshot)
{
	// 用攻击次数最多的武器决定原型，不要求过集中度阈值——
	// 原型回答"这层他主要在干什么"，集中度回答"他有多偏执"，两者是不同的问题
	FGameplayTag TopWeapon;
	int32 TopCount = 0;
	for (const TPair<FGameplayTag, int32>& Pair : Snapshot.WeaponAttackCounts)
	{
		if (Pair.Value > TopCount)
		{
			TopCount  = Pair.Value;
			TopWeapon = Pair.Key;
		}
	}

	const FGameplayTag BuildTag = GetBuildTagForWeapon(TopWeapon);

	if (BuildTag == SHMTags::Build_Ranged.GetTag())
	{
		return SHMTags::Archetype_Ranger.GetTag();
	}
	if (BuildTag == SHMTags::Build_Melee.GetTag())
	{
		return SHMTags::Archetype_Vanguard.GetTag();
	}

	// 没有攻击数据 → 无原型（无效标签）。
	// 下面的置信度计算会把"无原型"当作打法中断处理，不会误判为"延续"。
	return FGameplayTag();
}

// ============================================================================
//  置信度
//  连续同原型才升高；换打法或数据缺失则重置。
//  这一条规则的意义：玩家临时换把武器时，AI 不会立刻拿高置信度激进针对。
// ============================================================================
float FSHMProfileAnalyzer::ComputeConfidence(const FFloorBehaviorSnapshot& Current,
	const TArray<FFloorBehaviorSnapshot>& History)
{
	float Confidence = ConfidenceInitial;
	FGameplayTag PrevArchetype;
	bool bFirst = true;

	auto Accumulate = [&](const FFloorBehaviorSnapshot& Snapshot)
	{
		const FGameplayTag Archetype = ComputeDominantArchetype(Snapshot);

		if (bFirst)
		{
			Confidence = ConfidenceInitial;
			bFirst = false;
		}
		else if (Archetype.IsValid() && Archetype == PrevArchetype)
		{
			Confidence = FMath::Min(Confidence + ConfidenceStep, ConfidenceMax);
		}
		else
		{
			// 换打法 or 本层没数据 → 重新观察
			Confidence = ConfidenceInitial;
		}

		PrevArchetype = Archetype;
	};

	// 契约：History 只含往层，不含 Current，所以这里按时间顺序走一遍再算当前层
	for (const FFloorBehaviorSnapshot& Past : History)
	{
		Accumulate(Past);
	}
	Accumulate(Current);

	return Confidence;
}

namespace
{
	// 把一个值从 [WorstAt, BestAt] 区间线性映射到 0-100，并钳制。
	// 允许 WorstAt > BestAt（"越小越好"的指标，如清怪耗时）。
	float MapToScore(float Value, float WorstAt, float BestAt)
	{
		if (FMath::IsNearlyEqual(WorstAt, BestAt))
		{
			return FSHMProfileAnalyzer::NeutralScore;
		}
		const float Alpha = (WorstAt - Value) / (WorstAt - BestAt);
		return FMath::Clamp(Alpha * 100.f, 0.f, 100.f);
	}
}

// ============================================================================
//  战斗效率 = 清怪速度分 与 少挨打分 的均值
//  比率型维度：没有已清房间就没有分母，返回中性值而非 0
// ============================================================================
float FSHMProfileAnalyzer::ComputeCombatEfficiency(const FFloorBehaviorSnapshot& Snapshot)
{
	if (Snapshot.RoomsCleared <= 0)
	{
		return NeutralScore;
	}

	const float Rooms       = static_cast<float>(Snapshot.RoomsCleared);
	const float AvgClear    = Snapshot.TotalRoomClearTime / Rooms;
	const float HitsPerRoom = static_cast<float>(Snapshot.HitsTaken) / Rooms;

	const float TimeScore = MapToScore(AvgClear,    SlowClearSeconds, FastClearSeconds);
	const float HitScore  = MapToScore(HitsPerRoom, HitsPerRoomCeil,  HitsPerRoomFloor);

	return (TimeScore + HitScore) * 0.5f;
}

// ============================================================================
//  资源盈余 —— 当前 scope 内恒为中性值
//
//  为什么不按字段算：MVP 没有道具系统也没有弹药系统（Pick3 已砍，见 DECISIONS D-09），
//  HealItemsRemaining / AmmoRemaining 永远是 0。若照字段打分，会稳定输出"资源为零"，
//  导演据此以为玩家一贫如洗、进而加大补给——一个完全凭空的错误决策。
//
//  **没有数据源时，诚实地不打分，比打一个假分数安全。**
//  等道具系统落地后，把下面两行换成真实映射，并把对应测试改掉。
// ============================================================================
float FSHMProfileAnalyzer::ComputeResourceSurplus(const FFloorBehaviorSnapshot& Snapshot)
{
	return NeutralScore;
}

// ============================================================================
//  策略切换意愿 = 切换频率分 与 武器多样性分 的均值
//  比率型维度：完全没打过就没给过玩家换武器的机会，不能判他"不愿换"
// ============================================================================
float FSHMProfileAnalyzer::ComputeStrategySwitch(const FFloorBehaviorSnapshot& Snapshot)
{
	const bool bNoCombatData =
		Snapshot.WeaponAttackCounts.Num() == 0 &&
		Snapshot.UniqueWeaponsUsed.Num() == 0 &&
		Snapshot.WeaponSwitchCount == 0;

	if (bNoCombatData)
	{
		return NeutralScore;
	}

	const float SwitchScore = MapToScore(
		static_cast<float>(Snapshot.WeaponSwitchCount),
		0.f, static_cast<float>(SwitchCountForFullScore));

	// 用过的武器种类：1 种 → 0 分，达到 MVP 武器总数 → 满分
	const float VarietyScore = MapToScore(
		static_cast<float>(Snapshot.UniqueWeaponsUsed.Num()),
		1.f, static_cast<float>(MVPWeaponCount));

	return (SwitchScore + VarietyScore) * 0.5f;
}

// ============================================================================
//  生存压力 = 残血次数 × 权重 + 濒死次数 × 权重（钳制在 100）
//  计数型维度：没触发过事件本身就是有效观测（玩家很稳），返回 0 是正确的
// ============================================================================
float FSHMProfileAnalyzer::ComputeSurvivalPressure(const FFloorBehaviorSnapshot& Snapshot)
{
	const float Raw =
		static_cast<float>(Snapshot.LowHpEvents)      * LowHpEventWeight +
		static_cast<float>(Snapshot.DeathOrNearDeath) * NearDeathWeight;

	return FMath::Clamp(Raw, 0.f, 100.f);
}

// ============================================================================
//  主入口
// ============================================================================
FPlayerProfile FSHMProfileAnalyzer::Analyze(const FFloorBehaviorSnapshot& Current,
	const TArray<FFloorBehaviorSnapshot>& History)
{
	FPlayerProfile Profile;

	Profile.BuildConcentration = ComputeBuildConcentration(Current);
	Profile.CombatEfficiency   = ComputeCombatEfficiency(Current);
	Profile.ResourceSurplus    = ComputeResourceSurplus(Current);
	Profile.StrategySwitch     = ComputeStrategySwitch(Current);
	Profile.SurvivalPressure   = ComputeSurvivalPressure(Current);

	Profile.PrimaryBuildTags   = ComputePrimaryBuildTags(Current);
	Profile.DominantArchetype  = ComputeDominantArchetype(Current);
	Profile.Confidence         = ComputeConfidence(Current, History);

	return Profile;
}
