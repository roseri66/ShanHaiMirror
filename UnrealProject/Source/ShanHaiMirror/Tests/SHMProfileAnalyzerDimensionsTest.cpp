#include "Misc/AutomationTest.h"
#include "Director/SHMProfileAnalyzer.h"
#include "Framework/SHMCoreTypes.h"
#include "Framework/SHMGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// ProfileAnalyzer 五维评分测试（Build 集中度在 SHMProfileAnalyzerTest.cpp）
//
// 这批测试守的核心不变量：**"测不出来" 与 "表现差" 必须给出不同的分数。**
// 若两者都返回 0，导演会把一个还没开打的玩家当成正在挣扎的玩家来照顾，
// 这是画像层最隐蔽也最致命的一类错误——它不会崩，只会让 AI 一直做错决策。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags DimTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	using FAnalyzer = FSHMProfileAnalyzer;
}

// ============================================================================
//  战斗效率
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMCombatEfficiencyNoDataTest,
	"SHM.Director.ProfileAnalyzer.CombatEfficiency_NoRoomsIsNeutral", DimTestFlags)

bool FSHMCombatEfficiencyNoDataTest::RunTest(const FString& Parameters)
{
	// 一个房间都没清完：效率无从测量，必须给中性值而不是 0
	FFloorBehaviorSnapshot Snap;
	Snap.RoomsCleared = 0;

	TestEqual(TEXT("无已清房间时效率应为中性值，而非 0"),
		FAnalyzer::ComputeCombatEfficiency(Snap), FAnalyzer::NeutralScore);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMCombatEfficiencyHighTest,
	"SHM.Director.ProfileAnalyzer.CombatEfficiency_FastAndUnhurtIsHigh", DimTestFlags)

bool FSHMCombatEfficiencyHighTest::RunTest(const FString& Parameters)
{
	// 3 个房间，每个 12 秒清完（快于满分线），全程没挨打
	FFloorBehaviorSnapshot Snap;
	Snap.RoomsCleared = 3;
	Snap.TotalRoomClearTime = 36.f;
	Snap.HitsTaken = 0;

	TestEqual(TEXT("清得快且不挨打时效率应为满分"),
		FAnalyzer::ComputeCombatEfficiency(Snap), 100.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMCombatEfficiencyLowTest,
	"SHM.Director.ProfileAnalyzer.CombatEfficiency_SlowAndBeatenIsLow", DimTestFlags)

bool FSHMCombatEfficiencyLowTest::RunTest(const FString& Parameters)
{
	// 2 个房间，每个 120 秒（慢于 0 分线），每房挨打 10 次（超过 0 分线）
	FFloorBehaviorSnapshot Snap;
	Snap.RoomsCleared = 2;
	Snap.TotalRoomClearTime = 240.f;
	Snap.HitsTaken = 20;

	TestEqual(TEXT("清得慢且频繁挨打时效率应为 0"),
		FAnalyzer::ComputeCombatEfficiency(Snap), 0.f);

	return true;
}

// ============================================================================
//  资源盈余
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMResourceSurplusInertTest,
	"SHM.Director.ProfileAnalyzer.ResourceSurplus_InertWithoutItemSystem", DimTestFlags)

bool FSHMResourceSurplusInertTest::RunTest(const FString& Parameters)
{
	// 当前 scope 没有道具/弹药系统（Pick3 已砍，见 DECISIONS D-09），
	// 该维度没有真实数据源。此测试锁定"恒中性"这一决定，
	// 防止有人日后照着字段名写出一个基于恒零数据的假分数。
	FFloorBehaviorSnapshot Empty;
	TestEqual(TEXT("无道具系统时资源盈余应恒为中性值"),
		FAnalyzer::ComputeResourceSurplus(Empty), FAnalyzer::NeutralScore);

	// 即便字段被填了值，当前实现也不应据此打分——因为没有系统会去填它，
	// 出现非零值只可能是脏数据
	FFloorBehaviorSnapshot WithFakeData;
	WithFakeData.HealItemsRemaining = 5;
	WithFakeData.AmmoRemaining = 100;
	TestEqual(TEXT("字段有值也仍应返回中性值（该维度当前不参与决策）"),
		FAnalyzer::ComputeResourceSurplus(WithFakeData), FAnalyzer::NeutralScore);

	return true;
}

// ============================================================================
//  策略切换意愿
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMStrategySwitchOneTrickTest,
	"SHM.Director.ProfileAnalyzer.StrategySwitch_OneTrickIsZero", DimTestFlags)

bool FSHMStrategySwitchOneTrickTest::RunTest(const FString& Parameters)
{
	// 只用一把武器打了 100 下，一次没换——这是"一招鲜"的教科书画像
	FFloorBehaviorSnapshot Snap;
	Snap.WeaponAttackCounts.Add(SHMTags::Weapon_Bow, 100);
	Snap.UniqueWeaponsUsed.Add(SHMTags::Weapon_Bow);
	Snap.WeaponSwitchCount = 0;

	TestEqual(TEXT("只用一把武器且从不切换时切换意愿应为 0"),
		FAnalyzer::ComputeStrategySwitch(Snap), 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMStrategySwitchActiveTest,
	"SHM.Director.ProfileAnalyzer.StrategySwitch_ActiveSwitchingIsHigh", DimTestFlags)

bool FSHMStrategySwitchActiveTest::RunTest(const FString& Parameters)
{
	// 两把武器都在用，且切换次数达到满分线
	FFloorBehaviorSnapshot Snap;
	Snap.WeaponAttackCounts.Add(SHMTags::Weapon_Bow, 50);
	Snap.WeaponAttackCounts.Add(SHMTags::Weapon_Sword, 50);
	Snap.UniqueWeaponsUsed.Add(SHMTags::Weapon_Bow);
	Snap.UniqueWeaponsUsed.Add(SHMTags::Weapon_Sword);
	Snap.WeaponSwitchCount = FAnalyzer::SwitchCountForFullScore;

	TestEqual(TEXT("双武器 + 频繁切换时切换意愿应为满分"),
		FAnalyzer::ComputeStrategySwitch(Snap), 100.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMStrategySwitchNoDataTest,
	"SHM.Director.ProfileAnalyzer.StrategySwitch_NoCombatIsNeutral", DimTestFlags)

bool FSHMStrategySwitchNoDataTest::RunTest(const FString& Parameters)
{
	// 完全没打过：没给过玩家换武器的机会，不能判定他"不愿意换"
	FFloorBehaviorSnapshot Empty;

	TestEqual(TEXT("无任何武器使用时切换意愿应为中性值，而非 0"),
		FAnalyzer::ComputeStrategySwitch(Empty), FAnalyzer::NeutralScore);

	return true;
}

// ============================================================================
//  生存压力
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMSurvivalPressureNoEventsTest,
	"SHM.Director.ProfileAnalyzer.SurvivalPressure_NoEventsIsZero", DimTestFlags)

bool FSHMSurvivalPressureNoEventsTest::RunTest(const FString& Parameters)
{
	// 与比率型维度不同：没触发过残血本身就是有效观测（玩家很稳），
	// 这里返回 0 是正确的，不该给中性值
	FFloorBehaviorSnapshot Snap;
	Snap.LowHpEvents = 0;
	Snap.DeathOrNearDeath = 0;

	TestEqual(TEXT("无残血/濒死事件时生存压力应为 0"),
		FAnalyzer::ComputeSurvivalPressure(Snap), 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMSurvivalPressureHighTest,
	"SHM.Director.ProfileAnalyzer.SurvivalPressure_EventsAccumulate", DimTestFlags)

bool FSHMSurvivalPressureHighTest::RunTest(const FString& Parameters)
{
	// 残血 2 次 = 50
	FFloorBehaviorSnapshot Snap;
	Snap.LowHpEvents = 2;
	Snap.DeathOrNearDeath = 0;

	TestEqual(TEXT("残血两次应累计到 50"),
		FAnalyzer::ComputeSurvivalPressure(Snap), 50.f);

	// 残血 2 次 + 濒死 1 次 = 100（触顶）
	Snap.DeathOrNearDeath = 1;
	TestEqual(TEXT("再加一次濒死应达到满值 100"),
		FAnalyzer::ComputeSurvivalPressure(Snap), 100.f);

	// 继续叠加不应越界
	Snap.LowHpEvents = 20;
	Snap.DeathOrNearDeath = 10;
	TestEqual(TEXT("大量事件时应钳制在 100 而非溢出"),
		FAnalyzer::ComputeSurvivalPressure(Snap), 100.f);

	return true;
}

// ============================================================================
//  集成：Analyze 应填满五维
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMAnalyzePopulatesAllDimensionsTest,
	"SHM.Director.ProfileAnalyzer.Analyze_PopulatesAllFiveDimensions", DimTestFlags)

bool FSHMAnalyzePopulatesAllDimensionsTest::RunTest(const FString& Parameters)
{
	// 一个"远程站桩、打得顺、不换打法"的典型画像
	FFloorBehaviorSnapshot Snap;
	Snap.FloorIndex = 2;
	Snap.WeaponAttackCounts.Add(SHMTags::Weapon_Bow, 100);
	Snap.UniqueWeaponsUsed.Add(SHMTags::Weapon_Bow);
	Snap.WeaponSwitchCount = 0;
	Snap.RoomsCleared = 3;
	Snap.TotalRoomClearTime = 36.f;
	Snap.HitsTaken = 0;
	Snap.LowHpEvents = 0;

	const FPlayerProfile Profile = FAnalyzer::Analyze(Snap, {});

	TestEqual(TEXT("集中度：只用弓 → 100"), Profile.BuildConcentration, 100.f);
	TestEqual(TEXT("战斗效率：快且无伤 → 100"), Profile.CombatEfficiency, 100.f);
	TestEqual(TEXT("资源盈余：无数据源 → 中性"), Profile.ResourceSurplus, FAnalyzer::NeutralScore);
	TestEqual(TEXT("策略切换：一招鲜 → 0"), Profile.StrategySwitch, 0.f);
	TestEqual(TEXT("生存压力：没残过血 → 0"), Profile.SurvivalPressure, 0.f);

	TestTrue(TEXT("主导原型应为 Ranger"),
		Profile.DominantArchetype == SHMTags::Archetype_Ranger.GetTag());
	TestEqual(TEXT("首次判定置信度为初始值"), Profile.Confidence, FAnalyzer::ConfidenceInitial);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
