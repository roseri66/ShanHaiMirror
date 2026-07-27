#include "Misc/AutomationTest.h"
#include "Director/SHMJsonIntent.h"
#include "Director/SHMDirectorTypes.h"
#include "Framework/SHMGameplayTags.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// Intent JSON 解析测试
//
// 被测对象是"不信任 LLM 输出"的第一道关卡。LLM 会返回什么谁也不知道——
// 这批测试就是把"它可能怎么坑我"逐条钉死：畸形、缺字段、夹带数值、幻觉标签。
// 每一条都必须"安全失败或安全忽略"，绝不崩、绝不让脏数据穿透到玩法层。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags JsonTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	// LLM 应该返回的标准形态
	const TCHAR* ValidIntentJson = TEXT(R"({
		"challengeLevel": "Counter",
		"enemyWeights": { "Enemy.Grunt": 0.4, "Enemy.Tank": 0.35, "Enemy.Rush": 0.25 },
		"ruleIntents": [ { "tag": "Rule.Ammo", "level": "medium" } ],
		"narration": "你的弓用得很好。但这一层，别指望站在原地。",
		"reason": "连续三层远程站桩，高置信度。"
	})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMJsonValidTest,
	"SHM.Director.JsonIntent.Valid_ParsesAllFields", JsonTestFlags)
bool FSHMJsonValidTest::RunTest(const FString& Parameters)
{
	bool bOk = false;
	const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(ValidIntentJson, bOk);

	TestTrue(TEXT("合法 JSON 应解析成功"), bOk);
	TestTrue(TEXT("挑战等级应为 Counter"), Intent.ChallengeLevel == EChallengeLevel::Counter);
	TestEqual(TEXT("应解析出 3 个敌人权重"), Intent.EnemyWeights.Num(), 3);

	const float* TankW = Intent.EnemyWeights.Find(SHMTags::Enemy_Tank.GetTag());
	TestTrue(TEXT("Tank 权重应为 0.35"), TankW && FMath::IsNearlyEqual(*TankW, 0.35f, 0.001f));

	TestEqual(TEXT("应解析出 1 条规则意图"), Intent.RuleIntents.Num(), 1);
	if (Intent.RuleIntents.Num() == 1)
	{
		TestTrue(TEXT("规则标签应为 Rule.Ammo"),
			Intent.RuleIntents[0].RuleTag == FGameplayTag::RequestGameplayTag("Rule.Ammo"));
		TestEqual(TEXT("等级应为 medium"), Intent.RuleIntents[0].Level, FString(TEXT("medium")));
	}
	TestFalse(TEXT("台词不应为空"), Intent.Narration.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMJsonMalformedTest,
	"SHM.Director.JsonIntent.Malformed_FailsSafely", JsonTestFlags)
bool FSHMJsonMalformedTest::RunTest(const FString& Parameters)
{
	// LLM 常见翻车形态：截断、纯文本、空串、JSON 数组而非对象
	const TCHAR* BadInputs[] = {
		TEXT("{ \"challengeLevel\": \"Counter\", "),        // 截断
		TEXT("对不起，我无法完成这个请求。"),                  // 纯自然语言（模型拒答）
		TEXT(""),                                            // 空
		TEXT("[1,2,3]"),                                     // 数组不是对象
		TEXT("null"),
	};

	for (const TCHAR* Bad : BadInputs)
	{
		bool bOk = true;
		const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(Bad, bOk);
		TestFalse(FString::Printf(TEXT("畸形输入应判失败：%.20s"), Bad), bOk);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMJsonMissingFieldsTest,
	"SHM.Director.JsonIntent.MissingFields_UsesSafeDefaults", JsonTestFlags)
bool FSHMJsonMissingFieldsTest::RunTest(const FString& Parameters)
{
	// 结构合法但字段不全：解析应成功（有权重就能用），缺的部分取安全默认。
	// 判"能不能用"是护栏的事，解析器不越权。
	bool bOk = false;
	const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(
		TEXT(R"({ "enemyWeights": { "Enemy.Grunt": 1.0 } })"), bOk);

	TestTrue(TEXT("仅有权重时应解析成功"), bOk);
	TestEqual(TEXT("权重应被解析"), Intent.EnemyWeights.Num(), 1);
	TestEqual(TEXT("缺 ruleIntents 应为空数组而非崩溃"), Intent.RuleIntents.Num(), 0);
	TestTrue(TEXT("缺 challengeLevel 应取默认 Stable"), Intent.ChallengeLevel == EChallengeLevel::Stable);

	// 完全没有权重字段：无法构成可用决策，判失败
	bool bOk2 = true;
	FSHMJsonIntent::ParseFromJson(TEXT(R"({ "narration": "只有台词" })"), bOk2);
	TestFalse(TEXT("无敌人权重的 Intent 不可用，应判失败"), bOk2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMJsonRejectsNumbersTest,
	"SHM.Director.JsonIntent.NumericFields_AreDiscarded", JsonTestFlags)
bool FSHMJsonRejectsNumbersTest::RunTest(const FString& Parameters)
{
	// ★ 本项目核心不变量的第二重保险：LLM 擅自输出数值时，解析器直接不认。
	// 数值只能由 RuleResolver 查 DT_Rule 产生（D-15）。
	// 类型系统已经让 FDirectorIntent 装不下 multiplier，这里再确认解析层不会
	// 把它偷渡到别的字段上。
	bool bOk = false;
	const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(TEXT(R"({
		"enemyWeights": { "Enemy.Grunt": 1.0 },
		"ruleIntents": [ { "tag": "Rule.Ammo", "level": "medium", "multiplier": 0.1, "cost": 999 } ]
	})"), bOk);

	TestTrue(TEXT("含多余数值字段的 JSON 仍应解析成功（忽略而非报错）"), bOk);
	TestEqual(TEXT("规则应被解析"), Intent.RuleIntents.Num(), 1);
	if (Intent.RuleIntents.Num() == 1)
	{
		// 数值确实无处可去：FRuleIntent 只有 (标签, 等级) 两个字段。
		// 这正是 Intent/Decision 类型分离的意义——LLM 的数值在编译期就没有落点。
		TestTrue(TEXT("标签正常解析"),
			Intent.RuleIntents[0].RuleTag == FGameplayTag::RequestGameplayTag("Rule.Ammo"));
		TestEqual(TEXT("等级正常解析"), Intent.RuleIntents[0].Level, FString(TEXT("medium")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMJsonHallucinatedTagTest,
	"SHM.Director.JsonIntent.HallucinatedTag_ParsedThenGuarded", JsonTestFlags)
bool FSHMJsonHallucinatedTagTest::RunTest(const FString& Parameters)
{
	// LLM 编造了一个不存在的原型。解析器**不做业务判断**——它照常解析，
	// 由 Schema 护栏拒绝（白名单检查）。职责分离：解析器管结构，护栏管合法性。
	bool bOk = false;
	const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJson(TEXT(R"({
		"enemyWeights": { "Enemy.Dragon": 0.5, "Enemy.Grunt": 0.5 }
	})"), bOk);

	TestTrue(TEXT("含幻觉标签的 JSON 结构合法，应解析成功"), bOk);
	// 幻觉标签未在项目注册 → 解析成无效标签，Schema 护栏的白名单检查会拒它
	TestTrue(TEXT("已注册的 Grunt 应正常解析"),
		Intent.EnemyWeights.Contains(SHMTags::Enemy_Grunt.GetTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMJsonRoundTripTest,
	"SHM.Director.JsonIntent.RoundTrip_PreservesIntent", JsonTestFlags)
bool FSHMJsonRoundTripTest::RunTest(const FString& Parameters)
{
	// 决策日志写 rawIntent、回放 Provider 读回来——必须往返一致，
	// 否则录下来的决策回放出来是另一个东西
	FDirectorIntent Original;
	Original.ChallengeLevel = EChallengeLevel::Pressure;
	Original.EnemyWeights.Add(SHMTags::Enemy_Tank.GetTag(), 0.6f);
	Original.EnemyWeights.Add(SHMTags::Enemy_Grunt.GetTag(), 0.4f);
	FRuleIntent R; R.RuleTag = FGameplayTag::RequestGameplayTag("Rule.Cooldown"); R.Level = TEXT("light");
	Original.RuleIntents.Add(R);
	Original.Narration = TEXT("你走得太顺了。");
	Original.Reason    = TEXT("效率偏高。");

	bool bOk = false;
	const TSharedPtr<FJsonObject> Obj = FSHMJsonIntent::ToJsonObject(Original);
	const FDirectorIntent Back = FSHMJsonIntent::ParseFromJsonObject(Obj, bOk);

	TestTrue (TEXT("往返应成功"), bOk);
	TestTrue (TEXT("挑战等级应保持"), Back.ChallengeLevel == Original.ChallengeLevel);
	TestEqual(TEXT("权重条数应保持"), Back.EnemyWeights.Num(), Original.EnemyWeights.Num());
	TestEqual(TEXT("规则条数应保持"), Back.RuleIntents.Num(), Original.RuleIntents.Num());
	TestEqual(TEXT("台词应保持"), Back.Narration, Original.Narration);

	const float* TankW = Back.EnemyWeights.Find(SHMTags::Enemy_Tank.GetTag());
	TestTrue(TEXT("权重数值应保持"), TankW && FMath::IsNearlyEqual(*TankW, 0.6f, 0.001f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
