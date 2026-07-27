#include "Misc/AutomationTest.h"
#include "Director/SHMPromptBuilder.h"
#include "Director/SHMJsonIntent.h"
#include "Framework/SHMGameplayTags.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Prompt 构建测试
//
// 测的不是"LLM 会不会听话"（那不可控，所以才有护栏），而是
// **我们有没有把该说的约束说到位、该给的候选集给全**——这部分完全可控，必须测死。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags PromptTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	FDirectorContext MakePromptContext()
	{
		FDirectorContext Ctx;
		Ctx.FloorIndex      = 2;
		Ctx.TotalFloors     = 3;
		Ctx.ChallengeBudget = 55;

		Ctx.Profile.BuildConcentration = 90.f;
		Ctx.Profile.CombatEfficiency   = 85.f;
		Ctx.Profile.SurvivalPressure   = 10.f;
		Ctx.Profile.StrategySwitch     = 5.f;
		Ctx.Profile.Confidence         = 0.9f;
		Ctx.Profile.DominantArchetype  = SHMTags::Archetype_Ranger.GetTag();
		Ctx.Profile.PrimaryBuildTags   = { SHMTags::Build_Ranged.GetTag() };

		Ctx.AvailableArchetypes = {
			SHMTags::Enemy_Grunt.GetTag(), SHMTags::Enemy_Tank.GetTag(),
			SHMTags::Enemy_Rush.GetTag(),  SHMTags::Enemy_Shooter.GetTag() };

		FSHMAvailableRule R1;
		R1.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Ammo");
		R1.Level = TEXT("medium"); R1.Cost = 20;
		FSHMAvailableRule R2;
		R2.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Cooldown");
		R2.Level = TEXT("light"); R2.Cost = 10;
		Ctx.AvailableRules = { R1, R2 };

		return Ctx;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMPromptForbidsNumbersTest,
	"SHM.Director.Prompt.System_ForbidsNumericOutput", PromptTestFlags)
bool FSHMPromptForbidsNumbersTest::RunTest(const FString& Parameters)
{
	// ★ D-15 在自然语言层的表述。这条没说清楚，LLM 就会开始编数值——
	// 虽然类型系统和解析器都会挡住，但那会导致大量本可避免的降级。
	const FString Sys = FSHMPromptBuilder::BuildSystemPrompt();

	TestFalse(TEXT("system prompt 不应为空"), Sys.IsEmpty());
	TestTrue(TEXT("必须明令禁止输出数值"),
		Sys.Contains(TEXT("multiplier")) || Sys.Contains(TEXT("数值")));
	TestTrue(TEXT("必须要求严格 JSON 输出"),
		Sys.Contains(TEXT("JSON")));
	// 必须把输出 schema 的字段名讲清楚，否则解析全靠运气
	TestTrue(TEXT("应说明 enemyWeights 字段"), Sys.Contains(TEXT("enemyWeights")));
	TestTrue(TEXT("应说明 ruleIntents 字段"),  Sys.Contains(TEXT("ruleIntents")));
	TestTrue(TEXT("应说明 narration 字段"),    Sys.Contains(TEXT("narration")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMPromptInjectsContextTest,
	"SHM.Director.Prompt.User_InjectsCandidatesAndBudget", PromptTestFlags)
bool FSHMPromptInjectsContextTest::RunTest(const FString& Parameters)
{
	// 候选集必须完整注入——漏掉一个原型，LLM 就永远不会选它；
	// 漏掉预算，它就会给出必然被 Budget 护栏拒绝的方案（白白降级）
	const FDirectorContext Ctx = MakePromptContext();
	const FString User = FSHMPromptBuilder::BuildUserPrompt(Ctx);

	TestFalse(TEXT("user prompt 不应为空"), User.IsEmpty());

	// 四个候选原型一个都不能少
	TestTrue(TEXT("含 Enemy.Grunt"),   User.Contains(TEXT("Enemy.Grunt")));
	TestTrue(TEXT("含 Enemy.Tank"),    User.Contains(TEXT("Enemy.Tank")));
	TestTrue(TEXT("含 Enemy.Rush"),    User.Contains(TEXT("Enemy.Rush")));
	TestTrue(TEXT("含 Enemy.Shooter"), User.Contains(TEXT("Enemy.Shooter")));

	// 候选规则含标签与等级（LLM 要按 (标签,等级) 组合来选）
	TestTrue(TEXT("含 Rule.Ammo"),     User.Contains(TEXT("Rule.Ammo")));
	TestTrue(TEXT("含 Rule.Cooldown"), User.Contains(TEXT("Rule.Cooldown")));
	TestTrue(TEXT("含规则等级"),        User.Contains(TEXT("medium")));

	// 预算与画像
	TestTrue(TEXT("含挑战预算 55"), User.Contains(TEXT("55")));
	TestTrue(TEXT("含层号"),        User.Contains(TEXT("3")));   // total floors
	TestTrue(TEXT("含画像维度"),     User.Contains(TEXT("90")) || User.Contains(TEXT("集中度")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMPromptRequestBodyTest,
	"SHM.Director.Prompt.RequestBody_IsValidOpenAiShape", PromptTestFlags)
bool FSHMPromptRequestBodyTest::RunTest(const FString& Parameters)
{
	// 请求体自身必须是合法 JSON——拼错了会在服务端 400，而那是最难查的一类错误
	const FString Body = FSHMPromptBuilder::BuildRequestBody(MakePromptContext(), TEXT("test-model"));

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!TestTrue(TEXT("请求体应是合法 JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
	{
		return false;
	}

	FString Model;
	TestTrue (TEXT("含 model 字段"), Root->TryGetStringField(TEXT("model"), Model));
	TestEqual(TEXT("model 应为传入值"), Model, FString(TEXT("test-model")));

	const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
	if (TestTrue(TEXT("含 messages 数组"), Root->TryGetArrayField(TEXT("messages"), Messages)))
	{
		TestEqual(TEXT("应有 system + user 两条消息"), Messages->Num(), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMPromptDeterministicTest,
	"SHM.Director.Prompt.SameContext_SamePrompt", PromptTestFlags)
bool FSHMPromptDeterministicTest::RunTest(const FString& Parameters)
{
	// 纯函数保证：同 Context 必得同 prompt。
	// 若哪天引入了时间戳/随机数，这条会红——那是可测性退化的信号。
	const FDirectorContext Ctx = MakePromptContext();
	TestEqual(TEXT("两次构建应完全一致"),
		FSHMPromptBuilder::BuildRequestBody(Ctx, TEXT("m")),
		FSHMPromptBuilder::BuildRequestBody(Ctx, TEXT("m")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
