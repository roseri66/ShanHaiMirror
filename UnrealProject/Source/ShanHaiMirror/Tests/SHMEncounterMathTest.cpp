#include "Misc/AutomationTest.h"
#include "Combat/SHMEncounterMath.h"
#include "Framework/SHMGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// 遭遇数学测试 —— 这是"导演权重真实决定刷什么怪"的正确性保证。
// 抽样错了整个"定向反制"就是随机数，玩家感觉不到被针对。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags EncTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMEncPickBucketsTest,
	"SHM.Combat.Encounter.Pick_DeterministicBuckets", EncTestFlags)
bool FSHMEncPickBucketsTest::RunTest(const FString& Parameters)
{
	// 排序后（按标签名）：Grunt [0,0.5) → Rush [0.5,0.7) → Tank [0.7,1.0)
	TMap<FGameplayTag, float> W;
	W.Add(SHMTags::Enemy_Tank.GetTag(),  0.3f);   // 故意乱序塞入
	W.Add(SHMTags::Enemy_Grunt.GetTag(), 0.5f);
	W.Add(SHMTags::Enemy_Rush.GetTag(),  0.2f);

	TestTrue(TEXT("0.25 应落在 Grunt 桶"), FSHMEncounterMath::PickArchetype(W, 0.25f) == SHMTags::Enemy_Grunt.GetTag());
	TestTrue(TEXT("0.60 应落在 Rush 桶"),  FSHMEncounterMath::PickArchetype(W, 0.60f) == SHMTags::Enemy_Rush.GetTag());
	TestTrue(TEXT("0.90 应落在 Tank 桶"),  FSHMEncounterMath::PickArchetype(W, 0.90f) == SHMTags::Enemy_Tank.GetTag());
	TestTrue(TEXT("边界 0.0 应落在第一桶"), FSHMEncounterMath::PickArchetype(W, 0.f)  == SHMTags::Enemy_Grunt.GetTag());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMEncPickDegenerateTest,
	"SHM.Combat.Encounter.Pick_DegenerateInputsSafe", EncTestFlags)
bool FSHMEncPickDegenerateTest::RunTest(const FString& Parameters)
{
	// 空权重 / 全零权重：返回无效标签，不崩、不除零
	TMap<FGameplayTag, float> Empty;
	TestFalse(TEXT("空权重应返回无效标签"), FSHMEncounterMath::PickArchetype(Empty, 0.5f).IsValid());

	TMap<FGameplayTag, float> Zeros;
	Zeros.Add(SHMTags::Enemy_Grunt.GetTag(), 0.f);
	TestFalse(TEXT("全零权重应返回无效标签"), FSHMEncounterMath::PickArchetype(Zeros, 0.5f).IsValid());

	// Rand01 越界钳制：1.0（含）以上仍应落最后一桶而非越界
	TMap<FGameplayTag, float> W;
	W.Add(SHMTags::Enemy_Grunt.GetTag(), 1.f);
	TestTrue(TEXT("Rand=1.0 应钳到最后一桶"), FSHMEncounterMath::PickArchetype(W, 1.0f) == SHMTags::Enemy_Grunt.GetTag());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMEncBuildWaveTest,
	"SHM.Combat.Encounter.BuildWave_RespectsThreatBudget", EncTestFlags)
bool FSHMEncBuildWaveTest::RunTest(const FString& Parameters)
{
	TMap<FGameplayTag, float> W;
	W.Add(SHMTags::Enemy_Grunt.GetTag(), 0.5f);
	W.Add(SHMTags::Enemy_Tank.GetTag(),  0.3f);
	W.Add(SHMTags::Enemy_Rush.GetTag(),  0.2f);

	TMap<FGameplayTag, int32> Threat;
	Threat.Add(SHMTags::Enemy_Grunt.GetTag(), 5);
	Threat.Add(SHMTags::Enemy_Tank.GetTag(),  12);
	Threat.Add(SHMTags::Enemy_Rush.GetTag(),  8);

	FRandomStream Rand(42);   // 固定种子——波次可复现
	const TArray<FGameplayTag> Wave = FSHMEncounterMath::BuildWave(W, Threat, 30, Rand);

	TestTrue(TEXT("预算 30 至少能出一只怪"), Wave.Num() > 0);

	int32 Total = 0;
	for (const FGameplayTag& Tag : Wave)
	{
		TestTrue(TEXT("波次内标签全部有效"), Tag.IsValid());
		Total += Threat[Tag];
	}
	TestTrue(TEXT("Σ威胁不得超预算"), Total <= 30);
	TestTrue(TEXT("预算应被充分使用（余量小于最便宜的怪）"), 30 - Total < 5);

	// 同种子重跑结果一致（确定性）
	FRandomStream Rand2(42);
	const TArray<FGameplayTag> Wave2 = FSHMEncounterMath::BuildWave(W, Threat, 30, Rand2);
	TestTrue(TEXT("同种子波次应完全一致"), Wave == Wave2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
