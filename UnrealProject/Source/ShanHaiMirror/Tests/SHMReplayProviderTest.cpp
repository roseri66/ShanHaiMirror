#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Director/SHMReplayProvider.h"
#include "Director/SHMDecisionValidator.h"
#include "Framework/SHMGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// 回放 Provider 测试 —— 确定性是它存在的全部理由，所以确定性必须被测死
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags ReplayTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	// 写一个临时脚本文件，返回路径（测试结束不清理，位于 Saved/ 已被 gitignore）
	FString WriteTempScript(const FString& Name, const FString& Content)
	{
		const FString Path = FPaths::ProjectSavedDir() / TEXT("TestReplay") / Name;
		FFileHelper::SaveStringToFile(Content, *Path);
		return Path;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMReplayRealScriptTest,
	"SHM.Director.Replay.DefaultScript_LoadsAndProducesValidIntents", ReplayTestFlags)
bool FSHMReplayRealScriptTest::RunTest(const FString& Parameters)
{
	// 读真实入库的演示脚本——锁死它的格式，防止有人改坏了没人发现
	FSHMReplayProvider Provider(FPaths::ProjectDir() / TEXT("Data/ReplayScripts/Default.json"));

	if (!TestTrue(TEXT("默认回放脚本应能加载"), Provider.IsScriptLoaded()))
	{
		return false;
	}
	TestEqual(TEXT("演示脚本应有 3 层"), Provider.GetFloorCount(), 3);

	// 第 2 层应是定向反制：Tank 权重明显抬高
	FDirectorContext Ctx;
	Ctx.FloorIndex = 2;
	const FDirectorIntent Intent = Provider.RequestIntent(Ctx);

	TestTrue(TEXT("F2 应为定向反制等级"), Intent.ChallengeLevel == EChallengeLevel::Counter);
	const float* TankW = Intent.EnemyWeights.Find(SHMTags::Enemy_Tank.GetTag());
	TestTrue(TEXT("F2 应抬高 Tank 权重"), TankW && *TankW >= 0.3f);
	TestFalse(TEXT("F2 应有台词"), Intent.Narration.IsEmpty());

	// 权重和应为 1（Schema 护栏要求）——脚本写错会在这里暴露，而不是运行时才发现
	float Sum = 0.f;
	for (const TPair<FGameplayTag, float>& Pair : Intent.EnemyWeights) { Sum += Pair.Value; }
	TestTrue(TEXT("脚本里的权重和应为 1"), FMath::IsNearlyEqual(Sum, 1.f, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMReplayDeterminismTest,
	"SHM.Director.Replay.SameFloor_AlwaysSameIntent", ReplayTestFlags)
bool FSHMReplayDeterminismTest::RunTest(const FString& Parameters)
{
	// 回放的全部价值就是确定性：同一层反复请求必须完全一致
	FSHMReplayProvider Provider(FPaths::ProjectDir() / TEXT("Data/ReplayScripts/Default.json"));
	if (!Provider.IsScriptLoaded()) { return false; }

	FDirectorContext Ctx; Ctx.FloorIndex = 1;
	const FDirectorIntent A = Provider.RequestIntent(Ctx);
	const FDirectorIntent B = Provider.RequestIntent(Ctx);

	TestTrue (TEXT("挑战等级一致"), A.ChallengeLevel == B.ChallengeLevel);
	TestEqual(TEXT("台词一致"), A.Narration, B.Narration);
	TestEqual(TEXT("权重条数一致"), A.EnemyWeights.Num(), B.EnemyWeights.Num());
	for (const TPair<FGameplayTag, float>& Pair : A.EnemyWeights)
	{
		const float* Other = B.EnemyWeights.Find(Pair.Key);
		TestTrue(TEXT("每个权重值一致"), Other && FMath::IsNearlyEqual(*Other, Pair.Value, 0.0001f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMReplayBadScriptTest,
	"SHM.Director.Replay.BadScripts_FailSafely", ReplayTestFlags)
bool FSHMReplayBadScriptTest::RunTest(const FString& Parameters)
{
	// 文件不存在
	FSHMReplayProvider Missing(FPaths::ProjectDir() / TEXT("Data/ReplayScripts/__NotExist__.json"));
	TestFalse(TEXT("文件不存在应判未加载，不崩"), Missing.IsScriptLoaded());

	// 畸形 JSON
	FSHMReplayProvider Malformed(WriteTempScript(TEXT("Malformed.json"), TEXT("{ broken")));
	TestFalse(TEXT("畸形 JSON 应判未加载"), Malformed.IsScriptLoaded());

	// 缺 schemaVersion：拒绝加载（宁可不回放，也不能按错误格式跑出错误决策）
	FSHMReplayProvider NoVersion(WriteTempScript(TEXT("NoVersion.json"),
		TEXT(R"({ "floors": [ { "floorIndex": 0, "rawIntent": { "enemyWeights": {"Enemy.Grunt":1.0} } } ] })")));
	TestFalse(TEXT("缺 schemaVersion 应拒绝加载"), NoVersion.IsScriptLoaded());

	// 版本不匹配：同样拒绝
	FSHMReplayProvider WrongVersion(WriteTempScript(TEXT("WrongVersion.json"),
		TEXT(R"({ "schemaVersion": 999, "floors": [ { "floorIndex": 0, "rawIntent": { "enemyWeights": {"Enemy.Grunt":1.0} } } ] })")));
	TestFalse(TEXT("版本不匹配应拒绝加载"), WrongVersion.IsScriptLoaded());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMReplayMissingFloorTest,
	"SHM.Director.Replay.MissingFloor_ReturnsEmptyForDegrade", ReplayTestFlags)
bool FSHMReplayMissingFloorTest::RunTest(const FString& Parameters)
{
	// 脚本没录到的层：返回空 Intent 让上层降级，绝不猜一个凑数
	FSHMReplayProvider Provider(FPaths::ProjectDir() / TEXT("Data/ReplayScripts/Default.json"));
	if (!Provider.IsScriptLoaded()) { return false; }

	FDirectorContext Ctx; Ctx.FloorIndex = 99;
	const FDirectorIntent Intent = Provider.RequestIntent(Ctx);

	TestEqual(TEXT("缺失层应返回空权重（触发上层降级）"), Intent.EnemyWeights.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
