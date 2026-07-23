#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Misc/Paths.h"
#include "Director/SHMDirectorTypes.h"
#include "Director/SHMRuleResolver.h"
#include "Director/SHMDecisionValidator.h"
#include "Director/SHMLocalProvider.h"
#include "Framework/SHMGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// 决策层测试：RuleResolver（第⑥步）· DecisionValidator（第⑤步）· LocalProvider（第④步）
//
// 护栏测试是 DECISIONS §4.7 红线项：每道护栏必须有拒绝路径 + 通过路径。
// 全部测试不需要 World / PIE / 网络。
// ============================================================================

namespace SHMDecisionTest
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	// 内存中造一张与 Content/Data/RuleTable.csv 同构的规则表
	UDataTable* MakeRuleTable()
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = FSHMRuleRow::StaticStruct();

		auto AddRow = [Table](const TCHAR* Name, FGameplayTag Tag, const TCHAR* Level,
		                      float Mult, int32 Cost, FGameplayTag Conflict = FGameplayTag())
		{
			FSHMRuleRow Row;
			Row.RuleTag = Tag; Row.Level = Level; Row.Multiplier = Mult; Row.Cost = Cost;
			if (Conflict.IsValid()) { Row.ConflictsWith.AddTag(Conflict); }
			Table->AddRow(FName(Name), Row);
		};

		AddRow(TEXT("Ammo_light"),    FGameplayTag::RequestGameplayTag("Rule.Ammo"),         TEXT("light"),  0.85f, 10, FGameplayTag::RequestGameplayTag("Rule.RangedDamage"));
		AddRow(TEXT("Ammo_medium"),   FGameplayTag::RequestGameplayTag("Rule.Ammo"),         TEXT("medium"), 0.70f, 20, FGameplayTag::RequestGameplayTag("Rule.RangedDamage"));
		AddRow(TEXT("Cooldown_light"), FGameplayTag::RequestGameplayTag("Rule.Cooldown"),    TEXT("light"),  1.15f, 10);
		AddRow(TEXT("Cooldown_medium"),FGameplayTag::RequestGameplayTag("Rule.Cooldown"),    TEXT("medium"), 1.30f, 20);
		AddRow(TEXT("RangedDamage_medium"), FGameplayTag::RequestGameplayTag("Rule.RangedDamage"), TEXT("medium"), 0.80f, 20, FGameplayTag::RequestGameplayTag("Rule.Ammo"));
		return Table;
	}

	FSHMAvailableRule MakeAvail(const TCHAR* Tag, const TCHAR* Level, int32 Cost, const TCHAR* Conflict = nullptr)
	{
		FSHMAvailableRule R;
		R.RuleTag = FGameplayTag::RequestGameplayTag(Tag);
		R.Level = Level; R.Cost = Cost;
		if (Conflict) { R.ConflictsWith.AddTag(FGameplayTag::RequestGameplayTag(Conflict)); }
		return R;
	}

	// 标准候选集：与 CSV 数据同构
	FDirectorContext MakeContext(int32 Budget = 55, float Confidence = 0.9f)
	{
		FDirectorContext Ctx;
		Ctx.FloorIndex = 2; Ctx.TotalFloors = 3; Ctx.ChallengeBudget = Budget;
		Ctx.AvailableArchetypes = {
			SHMTags::Enemy_Grunt.GetTag(), SHMTags::Enemy_Tank.GetTag(),
			SHMTags::Enemy_Rush.GetTag(),  SHMTags::Enemy_Shooter.GetTag() };
		Ctx.AvailableRules = {
			MakeAvail(TEXT("Rule.Ammo"),         TEXT("light"),  10, TEXT("Rule.RangedDamage")),
			MakeAvail(TEXT("Rule.Ammo"),         TEXT("medium"), 20, TEXT("Rule.RangedDamage")),
			MakeAvail(TEXT("Rule.Cooldown"),     TEXT("light"),  10),
			MakeAvail(TEXT("Rule.Cooldown"),     TEXT("medium"), 20),
			MakeAvail(TEXT("Rule.Heal"),         TEXT("light"),  10),
			MakeAvail(TEXT("Rule.MeleeDamage"),  TEXT("medium"), 20),
			MakeAvail(TEXT("Rule.RangedDamage"), TEXT("medium"), 20, TEXT("Rule.Ammo")) };
		Ctx.Profile.Confidence = Confidence;
		return Ctx;
	}

	// 权重和为 1 的合法 Intent
	FDirectorIntent MakeValidIntent()
	{
		FDirectorIntent I;
		I.EnemyWeights.Add(SHMTags::Enemy_Grunt.GetTag(), 0.5f);
		I.EnemyWeights.Add(SHMTags::Enemy_Tank.GetTag(),  0.3f);
		I.EnemyWeights.Add(SHMTags::Enemy_Rush.GetTag(),  0.2f);
		FRuleIntent R; R.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Ammo"); R.Level = TEXT("medium");
		I.RuleIntents.Add(R);
		return I;
	}

	FPlayerProfile MakeRangerProfile(float Confidence = 0.9f)
	{
		FPlayerProfile P;
		P.BuildConcentration = 90.f; P.CombatEfficiency = 85.f;
		P.ResourceSurplus = 50.f; P.StrategySwitch = 5.f; P.SurvivalPressure = 10.f;
		P.DominantArchetype = SHMTags::Archetype_Ranger.GetTag();
		P.PrimaryBuildTags = { SHMTags::Build_Ranged.GetTag() };
		P.Confidence = Confidence;
		return P;
	}
}

using namespace SHMDecisionTest;

// ============================================================================
//  RuleResolver（第 ⑥ 步）
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMResolverKnownRuleTest,
	"SHM.Director.RuleResolver.KnownRule_MapsToMultiplier", Flags)
bool FSHMResolverKnownRuleTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeRuleTable();

	FRuleIntent Intent; Intent.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Ammo"); Intent.Level = TEXT("medium");
	const FRuleModifier Mod = FSHMRuleResolver::Resolve(Intent, Table);

	TestTrue(TEXT("解析结果应保留规则标签"), Mod.RuleTag == Intent.RuleTag);
	TestEqual(TEXT("(Ammo, medium) 应映射到 0.70"), Mod.Multiplier, 0.70f);
	TestEqual(TEXT("Cost 应从表拷贝"), Mod.Cost, 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMResolverUnknownRuleTest,
	"SHM.Director.RuleResolver.UnknownRule_ReturnsInvalidNoCrash", Flags)
bool FSHMResolverUnknownRuleTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeRuleTable();

	FRuleIntent Intent; Intent.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Crit"); Intent.Level = TEXT("medium");
	const FRuleModifier Mod = FSHMRuleResolver::Resolve(Intent, Table);

	TestFalse(TEXT("表中不存在的规则应返回无效标签而非崩溃"), Mod.RuleTag.IsValid());

	// 空表指针同样不崩
	const FRuleModifier ModNull = FSHMRuleResolver::Resolve(Intent, nullptr);
	TestFalse(TEXT("空表也应安全返回无效"), ModNull.RuleTag.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMResolverCsvFileTest,
	"SHM.Director.RuleResolver.CsvFile_LoadsAndResolves", Flags)
bool FSHMResolverCsvFileTest::RunTest(const FString& Parameters)
{
	// 加载真实入库的 CSV，锁死文件格式（含 GameplayTagContainer 的转义写法）
	const FString Path = FPaths::ProjectContentDir() / TEXT("Data/RuleTable.csv");
	UDataTable* Table = FSHMRuleResolver::LoadTableFromCsvFile(Path, GetTransientPackage());

	if (!TestNotNull(TEXT("RuleTable.csv 应能加载"), Table)) { return false; }

	FRuleIntent Intent; Intent.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Ammo"); Intent.Level = TEXT("medium");
	const FRuleModifier Mod = FSHMRuleResolver::Resolve(Intent, Table);
	TestEqual(TEXT("真实 CSV 中 (Ammo, medium) 应为 0.70"), Mod.Multiplier, 0.70f);

	// 冲突信息也要能从 CSV 读出（验证容器列的转义格式没写错）
	bool bFoundConflict = false;
	Table->ForeachRow<FSHMRuleRow>(TEXT("test"), [&](const FName&, const FSHMRuleRow& Row)
	{
		if (Row.RuleTag == Intent.RuleTag && Row.Level == TEXT("medium"))
		{
			bFoundConflict = Row.ConflictsWith.HasTag(FGameplayTag::RequestGameplayTag("Rule.RangedDamage"));
		}
	});
	TestTrue(TEXT("Ammo_medium 的 ConflictsWith 应含 Rule.RangedDamage"), bFoundConflict);
	return true;
}

// ============================================================================
//  DecisionValidator（第 ⑤ 步）—— 四道护栏，各一拒一过
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorSchemaWeightsTest,
	"SHM.Director.Validator.Schema_WeightsMustSumToOne", Flags)
bool FSHMValidatorSchemaWeightsTest::RunTest(const FString& Parameters)
{
	FDirectorContext Ctx = MakeContext();
	FDirectorIntent Intent = MakeValidIntent();
	Intent.EnemyWeights.Add(SHMTags::Enemy_Grunt.GetTag(), 0.1f); // 破坏权重和

	FValidationResult R; FSHMDecisionValidator::CheckSchema(Intent, Ctx, R);
	TestFalse(TEXT("权重和≠1 应被拒绝"), R.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorSchemaWhitelistTest,
	"SHM.Director.Validator.Schema_TagsMustBeWhitelisted", Flags)
bool FSHMValidatorSchemaWhitelistTest::RunTest(const FString& Parameters)
{
	FDirectorContext Ctx = MakeContext();

	// 不在候选集里的敌人原型（Boss 不在 4 原型白名单）
	FDirectorIntent Intent1 = MakeValidIntent();
	Intent1.EnemyWeights.Empty();
	Intent1.EnemyWeights.Add(FGameplayTag::RequestGameplayTag("Enemy.Boss"), 1.0f);
	FValidationResult R1; FSHMDecisionValidator::CheckSchema(Intent1, Ctx, R1);
	TestFalse(TEXT("白名单外的原型应被拒绝"), R1.bValid);

	// 不在候选集里的规则（Heal 只开放了 light，medium 未开放）
	FDirectorIntent Intent2 = MakeValidIntent();
	Intent2.RuleIntents.Empty();
	FRuleIntent RI; RI.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Heal"); RI.Level = TEXT("medium");
	Intent2.RuleIntents.Add(RI);
	FValidationResult R2; FSHMDecisionValidator::CheckSchema(Intent2, Ctx, R2);
	TestFalse(TEXT("候选集外的规则(标签+等级)应被拒绝"), R2.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorBudgetTest,
	"SHM.Director.Validator.Budget_SumCostWithinBudget", Flags)
bool FSHMValidatorBudgetTest::RunTest(const FString& Parameters)
{
	// 预算 20，塞两条 medium（20+20=40）→ 拒
	FDirectorContext Ctx = MakeContext(/*Budget=*/20);
	FDirectorIntent Intent = MakeValidIntent(); // 含 Ammo_medium(20)
	FRuleIntent Extra; Extra.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Cooldown"); Extra.Level = TEXT("medium");
	Intent.RuleIntents.Add(Extra);

	FValidationResult R; FSHMDecisionValidator::CheckBudget(Intent, Ctx, R);
	TestFalse(TEXT("Σcost 超预算应被拒绝"), R.bValid);

	// 预算 55 → 过
	FDirectorContext CtxOk = MakeContext(/*Budget=*/55);
	FValidationResult ROk; FSHMDecisionValidator::CheckBudget(Intent, CtxOk, ROk);
	TestTrue(TEXT("Σcost 在预算内应通过"), ROk.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorConflictTest,
	"SHM.Director.Validator.Conflict_ExclusivePairRejected", Flags)
bool FSHMValidatorConflictTest::RunTest(const FString& Parameters)
{
	FDirectorContext Ctx = MakeContext();

	// 弹药↓ + 远程伤害↓ 同时出现 = 把远程 Build 逼入无解，设计上互斥
	FDirectorIntent Intent = MakeValidIntent(); // 含 Ammo_medium
	FRuleIntent Bad; Bad.RuleTag = FGameplayTag::RequestGameplayTag("Rule.RangedDamage"); Bad.Level = TEXT("medium");
	Intent.RuleIntents.Add(Bad);

	FValidationResult R; FSHMDecisionValidator::CheckConflict(Intent, Ctx, R);
	TestFalse(TEXT("互斥规则对应被拒绝"), R.bValid);

	// 单独一条不冲突
	FDirectorIntent Ok = MakeValidIntent();
	FValidationResult ROk; FSHMDecisionValidator::CheckConflict(Ok, Ctx, ROk);
	TestTrue(TEXT("无互斥对应通过"), ROk.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorFairnessConsecutiveTest,
	"SHM.Director.Validator.Fairness_ThirdConsecutiveFloorRejected", Flags)
bool FSHMValidatorFairnessConsecutiveTest::RunTest(const FString& Parameters)
{
	FDirectorContext Ctx = MakeContext();

	// 弹药规则已连续用了 2 层（F0、F1），本层（F2）再用 = 连续第 3 层 → 拒
	const FGameplayTag Ammo = FGameplayTag::RequestGameplayTag("Rule.Ammo");
	FDirectorHistoryEntry H0; H0.FloorIndex = 0; H0.AppliedRuleTags = { Ammo };
	FDirectorHistoryEntry H1; H1.FloorIndex = 1; H1.AppliedRuleTags = { Ammo };
	Ctx.DecisionHistory = { H0, H1 };

	FDirectorIntent Intent = MakeValidIntent(); // 含 Ammo
	FValidationResult R; FSHMDecisionValidator::CheckFairness(Intent, Ctx, R);
	TestFalse(TEXT("同一规则连续第 3 层应被拒绝"), R.bValid);

	// 只连续 1 层则允许
	Ctx.DecisionHistory = { H1 };
	FValidationResult ROk; FSHMDecisionValidator::CheckFairness(Intent, Ctx, ROk);
	TestTrue(TEXT("仅连续第 2 层应通过"), ROk.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorFairnessHeavyTest,
	"SHM.Director.Validator.Fairness_HeavyNeedsConfidence", Flags)
bool FSHMValidatorFairnessHeavyTest::RunTest(const FString& Parameters)
{
	// 置信度 0.5 < 0.6，出重度规则 → 拒（"玩家临时换武器不该被激进针对"的最后防线）
	FDirectorContext Ctx = MakeContext(/*Budget=*/55, /*Confidence=*/0.5f);
	Ctx.AvailableRules.Add(MakeAvail(TEXT("Rule.Cooldown"), TEXT("heavy"), 30));

	FDirectorIntent Intent = MakeValidIntent();
	Intent.RuleIntents.Empty();
	FRuleIntent Heavy; Heavy.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Cooldown"); Heavy.Level = TEXT("heavy");
	Intent.RuleIntents.Add(Heavy);

	FValidationResult R; FSHMDecisionValidator::CheckFairness(Intent, Ctx, R);
	TestFalse(TEXT("低置信度 + 重度规则应被拒绝"), R.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMValidatorAllPassTest,
	"SHM.Director.Validator.ValidIntent_PassesAllFourGates", Flags)
bool FSHMValidatorAllPassTest::RunTest(const FString& Parameters)
{
	// 通过路径：合法 Intent 过全部四道护栏
	FDirectorContext Ctx = MakeContext();
	const FValidationResult R = FSHMDecisionValidator::Validate(MakeValidIntent(), Ctx);

	TestTrue(TEXT("合法 Intent 应通过全部护栏"), R.bValid);
	TestEqual(TEXT("不应有任何拒绝原因"), R.RejectReasons.Num(), 0);
	return true;
}

// ============================================================================
//  LocalProvider（第 ④ 步）
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMLocalProviderCounterTest,
	"SHM.Director.LocalProvider.RangerProfile_GetsCounterWeights", Flags)
bool FSHMLocalProviderCounterTest::RunTest(const FString& Parameters)
{
	// 远程站桩 + 高置信度 → 定向反制：Tank(逼近) + Rush(打断) 权重必须为正，
	// 且产出必须能过自家护栏（含互斥：选了 Ammo 就不得再选 RangedDamage）
	FDirectorContext Ctx = MakeContext();
	Ctx.Profile = MakeRangerProfile(0.9f);

	FSHMLocalProvider Provider;
	const FDirectorIntent Intent = Provider.RequestIntent(Ctx);

	const float* TankW = Intent.EnemyWeights.Find(SHMTags::Enemy_Tank.GetTag());
	const float* RushW = Intent.EnemyWeights.Find(SHMTags::Enemy_Rush.GetTag());
	TestTrue(TEXT("应提高 Tank 权重"), TankW && *TankW > 0.f);
	TestTrue(TEXT("应提高 Rush 权重"), RushW && *RushW > 0.f);

	bool bHasAmmo = false;
	for (const FRuleIntent& R : Intent.RuleIntents)
	{
		if (R.RuleTag == FGameplayTag::RequestGameplayTag("Rule.Ammo")) { bHasAmmo = true; }
	}
	TestTrue(TEXT("应选择弹药规则针对远程"), bHasAmmo);

	const FValidationResult VR = FSHMDecisionValidator::Validate(Intent, Ctx);
	TestTrue(TEXT("本地 Provider 的产出必须通过全部护栏"), VR.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMLocalProviderRecoveryTest,
	"SHM.Director.LocalProvider.HighPressure_GetsRecovery", Flags)
bool FSHMLocalProviderRecoveryTest::RunTest(const FString& Parameters)
{
	// 生存压力高 → 恢复期：只出杂兵、零规则（GDD §6.2「挣扎中」行为）
	FDirectorContext Ctx = MakeContext();
	Ctx.Profile = MakeRangerProfile(0.9f);
	Ctx.Profile.SurvivalPressure = 80.f;

	FSHMLocalProvider Provider;
	const FDirectorIntent Intent = Provider.RequestIntent(Ctx);

	TestEqual(TEXT("恢复期不应有任何规则"), Intent.RuleIntents.Num(), 0);
	const float* GruntW = Intent.EnemyWeights.Find(SHMTags::Enemy_Grunt.GetTag());
	TestTrue(TEXT("恢复期应全部为杂兵"), GruntW && FMath::IsNearlyEqual(*GruntW, 1.f, 0.01f));
	TestTrue(TEXT("挑战等级应为恢复期"), Intent.ChallengeLevel == EChallengeLevel::Recovery);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMLocalProviderLowConfidenceTest,
	"SHM.Director.LocalProvider.LowConfidence_OnlyLightRules", Flags)
bool FSHMLocalProviderLowConfidenceTest::RunTest(const FString& Parameters)
{
	// 置信度低（刚换打法）→ 最多轻度调整（TDD §3.2 置信度规则的 Provider 侧体现）
	FDirectorContext Ctx = MakeContext(/*Budget=*/55, /*Confidence=*/0.5f);
	Ctx.Profile = MakeRangerProfile(0.5f);

	FSHMLocalProvider Provider;
	const FDirectorIntent Intent = Provider.RequestIntent(Ctx);

	for (const FRuleIntent& R : Intent.RuleIntents)
	{
		TestEqual(TEXT("低置信度下只允许轻度规则"), R.Level, FString(TEXT("light")));
	}
	TestTrue(TEXT("低置信度下规则不超过 1 条"), Intent.RuleIntents.Num() <= 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
