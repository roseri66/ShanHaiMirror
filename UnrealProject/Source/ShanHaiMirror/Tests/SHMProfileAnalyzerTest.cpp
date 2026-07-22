#include "Misc/AutomationTest.h"
#include "Director/SHMProfileAnalyzer.h"
#include "Framework/SHMCoreTypes.h"
#include "Framework/SHMGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// ProfileAnalyzer 单元测试
//
// 跑法：
//   编辑器  Tools → Session Frontend → Automation → 勾 SHM.Director → Start
//   命令行  UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests SHM.Director"
//           -unattended -nopause -testexit="Automation Test Queue Empty"
//
// 这些测试**不需要 World、不需要 PIE、不需要网络**——因为被测对象是纯函数。
// 如果哪天某个测试开始需要起 PIE 了，说明 ProfileAnalyzer 的纯粹性被破坏了，
// 那是设计退化的信号，不是"把测试改复杂点"就能解决的问题。
// ============================================================================

namespace
{
	// 注意类型：UE 5.5 起 EAutomationTestFlags 是 enum class（旧版是 namespace 里的 uint32
	// 常量）。写成 int32 会编译不过——网上多数示例还是旧写法。
	constexpr EAutomationTestFlags TestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	// 造一个带指定武器攻击次数的快照
	FFloorBehaviorSnapshot MakeSnapshot(int32 FloorIndex,
		const TMap<FGameplayTag, int32>& WeaponAttacks)
	{
		FFloorBehaviorSnapshot Snap;
		Snap.FloorIndex = FloorIndex;
		Snap.WeaponAttackCounts = WeaponAttacks;
		for (const TPair<FGameplayTag, int32>& Pair : WeaponAttacks)
		{
			Snap.UniqueWeaponsUsed.Add(Pair.Key);
		}
		return Snap;
	}
}

// ============================================================================
// 用例 1：零攻击 —— 不崩溃、不除零、集中度为 0
// 这是最重要的一个测试：玩家第一层可能一次没打就走到出口，
// 除零会让整条导演链路在第一层就挂掉。
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMProfileAnalyzerZeroAttackTest,
	"SHM.Director.ProfileAnalyzer.ZeroAttack_NoDivideByZero", TestFlags)

bool FSHMProfileAnalyzerZeroAttackTest::RunTest(const FString& Parameters)
{
	const FFloorBehaviorSnapshot Empty = MakeSnapshot(0, {});

	const float Concentration = FSHMProfileAnalyzer::ComputeBuildConcentration(Empty);
	TestEqual(TEXT("零攻击时集中度应为 0"), Concentration, 0.f);

	const TArray<FGameplayTag> Primary = FSHMProfileAnalyzer::ComputePrimaryBuildTags(Empty);
	TestEqual(TEXT("零攻击时不应有主力打法标签"), Primary.Num(), 0);

	// 完整链路也要能安全跑完
	const FPlayerProfile Profile = FSHMProfileAnalyzer::Analyze(Empty, {});
	TestEqual(TEXT("零攻击时画像集中度应为 0"), Profile.BuildConcentration, 0.f);

	return true;
}

// ============================================================================
// 用例 2：单一武器 100% —— 集中度 100，该武器的 Build 标签进 PrimaryBuildTags
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMProfileAnalyzerSingleWeaponTest,
	"SHM.Director.ProfileAnalyzer.SingleWeapon_FullConcentration", TestFlags)

bool FSHMProfileAnalyzerSingleWeaponTest::RunTest(const FString& Parameters)
{
	const FFloorBehaviorSnapshot Snap = MakeSnapshot(0, {
		{ SHMTags::Weapon_Bow, 100 }
	});

	TestEqual(TEXT("只用一把武器时集中度应为 100"),
		FSHMProfileAnalyzer::ComputeBuildConcentration(Snap), 100.f);

	const TArray<FGameplayTag> Primary = FSHMProfileAnalyzer::ComputePrimaryBuildTags(Snap);
	TestEqual(TEXT("应识别出一个主力打法标签"), Primary.Num(), 1);
	if (Primary.Num() == 1)
	{
		TestTrue(TEXT("弓应映射为 Build.Ranged"),
			Primary[0] == SHMTags::Build_Ranged.GetTag());
	}

	return true;
}

// ============================================================================
// 用例 3：两把武器 50/50 —— 集中度 50，未过阈值故 PrimaryBuildTags 为空
// 这个用例守的是"AI 不该针对一个没有明显偏好的玩家"这条设计意图。
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMProfileAnalyzerBalancedWeaponTest,
	"SHM.Director.ProfileAnalyzer.BalancedWeapons_NoPrimaryBuild", TestFlags)

bool FSHMProfileAnalyzerBalancedWeaponTest::RunTest(const FString& Parameters)
{
	const FFloorBehaviorSnapshot Snap = MakeSnapshot(0, {
		{ SHMTags::Weapon_Bow,   50 },
		{ SHMTags::Weapon_Sword, 50 }
	});

	TestEqual(TEXT("五五开时集中度应为 50"),
		FSHMProfileAnalyzer::ComputeBuildConcentration(Snap), 50.f);

	TestEqual(TEXT("集中度未过阈值时不应产出主力打法标签"),
		FSHMProfileAnalyzer::ComputePrimaryBuildTags(Snap).Num(), 0);

	return true;
}

// ============================================================================
// 用例 4：空历史（第 1 层）—— 置信度为初始值
// 第一层 AI 只观察不调整，靠的就是这里的低置信度。
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMProfileAnalyzerEmptyHistoryTest,
	"SHM.Director.ProfileAnalyzer.EmptyHistory_InitialConfidence", TestFlags)

bool FSHMProfileAnalyzerEmptyHistoryTest::RunTest(const FString& Parameters)
{
	const FFloorBehaviorSnapshot Snap = MakeSnapshot(0, {
		{ SHMTags::Weapon_Bow, 100 }
	});

	TestEqual(TEXT("无历史时置信度应为初始值 0.5"),
		FSHMProfileAnalyzer::ComputeConfidence(Snap, {}),
		FSHMProfileAnalyzer::ConfidenceInitial);

	return true;
}

// ============================================================================
// 用例 5：连续 3 层同原型 —— 置信度递增到 0.9
// 0.5 → +0.2（第2层同）→ +0.2（第3层同）= 0.9
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMProfileAnalyzerConfidenceGrowthTest,
	"SHM.Director.ProfileAnalyzer.SameArchetype_ConfidenceGrows", TestFlags)

bool FSHMProfileAnalyzerConfidenceGrowthTest::RunTest(const FString& Parameters)
{
	// 前两层都是远程
	TArray<FFloorBehaviorSnapshot> History;
	History.Add(MakeSnapshot(0, { { SHMTags::Weapon_Bow, 100 } }));
	History.Add(MakeSnapshot(1, { { SHMTags::Weapon_Bow, 100 } }));

	// 当前第三层还是远程
	const FFloorBehaviorSnapshot Current = MakeSnapshot(2, { { SHMTags::Weapon_Bow, 100 } });

	TestEqual(TEXT("连续三层同打法置信度应为 0.9"),
		FSHMProfileAnalyzer::ComputeConfidence(Current, History), 0.9f);

	return true;
}

// ============================================================================
// 用例 6：中途换原型 —— 置信度重置
// 这条守的是体验红线：玩家临时换了把武器，AI 不该立刻拿高置信度去激进针对。
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMProfileAnalyzerConfidenceResetTest,
	"SHM.Director.ProfileAnalyzer.ArchetypeChange_ConfidenceResets", TestFlags)

bool FSHMProfileAnalyzerConfidenceResetTest::RunTest(const FString& Parameters)
{
	// 前两层远程，置信度本已涨到 0.7
	TArray<FFloorBehaviorSnapshot> History;
	History.Add(MakeSnapshot(0, { { SHMTags::Weapon_Bow, 100 } }));
	History.Add(MakeSnapshot(1, { { SHMTags::Weapon_Bow, 100 } }));

	// 第三层改近战
	const FFloorBehaviorSnapshot Current = MakeSnapshot(2, { { SHMTags::Weapon_Sword, 100 } });

	TestEqual(TEXT("换打法后置信度应重置为 0.5"),
		FSHMProfileAnalyzer::ComputeConfidence(Current, History),
		FSHMProfileAnalyzer::ConfidenceInitial);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
