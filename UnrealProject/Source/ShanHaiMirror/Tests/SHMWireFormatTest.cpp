#include "Misc/AutomationTest.h"
#include "Director/SHMDirectorWireFormat.h"
#include "Director/SHMDecisionLogFormat.h"
#include "Framework/SHMGameplayTags.h"
#include "Dom/JsonObject.h"

// ============================================================================
// 上行契约（D-23）与序列化重构的回归测试
//
// 这一组守两件事：
//
// ① **上行契约的取值域**。踩坑 #22 的教训是"契约只定死字段名、没定死取值域，
//    于是取值跟着实现细节漂移"——`UEnum::GetValueAsString()` 把
//    "ESHMGuardrail::Conflict" 写进了下行日志。上行的消费方是 Java，
//    同一个错误会原封不动地重演一次，所以这里有一条专门扫 C++ 类型名泄漏的测试。
//
// ② **重构不改变决策日志的输出**。把 RecordLogEntry 里手拼的 JSON 抽成共用函数
//    是纯重构，日志的 profile 与 context 两块必须与重构前**逐字段一致**。
//    下面的 golden 值直接取自重构前 SHMDirectorCore.cpp 的字面量。
//
// 命名注意：UE 默认开 unity build，匿名命名空间会被合并，"文件内私有"是假的
// （踩坑 #25）。故本文件所有辅助函数带 Wire 前缀。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags WireTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	// 一个字段全部非默认值的画像——默认值测不出"字段有没有真的被写进去"
	FPlayerProfile WireMakeProfile()
	{
		FPlayerProfile P;
		P.BuildConcentration = 87.f;
		P.CombatEfficiency   = 72.f;
		P.ResourceSurplus    = 40.f;
		P.StrategySwitch     = 15.f;
		P.SurvivalPressure   = 22.f;
		P.Confidence         = 0.9f;
		P.DominantArchetype  = SHMTags::Archetype_Ranger.GetTag();
		// 非空：prompt 的「主力打法」一行读它，空数组测不出字段有没有真的写进去
		P.PrimaryBuildTags   = { SHMTags::Build_Ranged.GetTag() };
		return P;
	}

	FDirectorContext WireMakeContext()
	{
		FDirectorContext C;
		C.Profile         = WireMakeProfile();
		C.FloorIndex      = 1;
		C.TotalFloors     = 3;
		C.ChallengeBudget = 55;

		FSHMAvailableRule Ammo;
		Ammo.RuleTag = SHMTags::Rule_Ammo.GetTag();
		Ammo.Level   = TEXT("medium");
		Ammo.Cost    = 20;
		// 非空互斥：prompt 要注入它，空容器测不出字段有没有真的写进去。
		// 这一对（弹药↓ / 远程伤害↓）正是实测撞出 Conflict 拦截的那一对。
		Ammo.ConflictsWith.AddTag(SHMTags::Rule_RangedDamage.GetTag());
		C.AvailableRules.Add(Ammo);

		C.AvailableArchetypes.Add(SHMTags::Enemy_Grunt.GetTag());
		C.AvailableArchetypes.Add(SHMTags::Enemy_Tank.GetTag());

		FDirectorHistoryEntry H;
		H.FloorIndex = 0;
		H.AppliedRuleTags.Add(SHMTags::Rule_Cooldown.GetTag());
		C.DecisionHistory.Add(H);

		return C;
	}
}

// ---------------------------------------------------------------------------
// 画像块：七个字段，字段名与重构前的字面量逐一对齐
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireProfileFieldsTest,
	"SHM.Director.Wire.Profile_MatchesPreRefactorFieldNames", WireTestFlags)

bool FWireProfileFieldsTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> J = FSHMDirectorWire::ProfileToJson(WireMakeProfile());
	if (!TestNotNull(TEXT("画像 JSON 不应为空"), J.Get())) { return false; }

	// golden：重构前 RecordLogEntry 里的七行字面量
	TestTrue(TEXT("应有 buildConcentration"), J->HasField(TEXT("buildConcentration")));
	TestTrue(TEXT("应有 combatEfficiency"),   J->HasField(TEXT("combatEfficiency")));
	TestTrue(TEXT("应有 resourceSurplus"),    J->HasField(TEXT("resourceSurplus")));
	TestTrue(TEXT("应有 strategySwitch"),     J->HasField(TEXT("strategySwitch")));
	TestTrue(TEXT("应有 survivalPressure"),   J->HasField(TEXT("survivalPressure")));
	TestTrue(TEXT("应有 confidence"),         J->HasField(TEXT("confidence")));
	TestTrue(TEXT("应有 dominantArchetype"),  J->HasField(TEXT("dominantArchetype")));

	TestEqual(TEXT("画像块恰好七个字段，多一个都是契约外的"), J->Values.Num(), 7);

	TestEqual(TEXT("buildConcentration 取值应原样写入"),
		J->GetNumberField(TEXT("buildConcentration")), 87.0);
	TestEqual(TEXT("confidence 是 0-1 量纲，不应被当成 0-100 缩放"),
		J->GetNumberField(TEXT("confidence")), 0.9, 0.0001);
	TestEqual(TEXT("dominantArchetype 应写完整 Tag 名"),
		J->GetStringField(TEXT("dominantArchetype")), FString(TEXT("Archetype.Ranger")));

	return true;
}

// ---------------------------------------------------------------------------
// 日志 context 块：刻意只有两个字段，重构不得改变它
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireLogContextShapeTest,
	"SHM.Director.Wire.LogContext_StillExactlyTwoFields", WireTestFlags)

bool FWireLogContextShapeTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> J = FSHMDirectorWire::LogContextToJson(WireMakeContext());
	if (!TestNotNull(TEXT("context JSON 不应为空"), J.Get())) { return false; }

	TestEqual(TEXT("日志 context 块恰好两个字段（重构前如此，重构后必须一样）"),
		J->Values.Num(), 2);
	TestTrue(TEXT("应有 challengeBudget"), J->HasField(TEXT("challengeBudget")));
	TestTrue(TEXT("应有 availableRules"),  J->HasField(TEXT("availableRules")));

	TestEqual(TEXT("challengeBudget 取值应原样写入"),
		J->GetNumberField(TEXT("challengeBudget")), 55.0);

	const TArray<TSharedPtr<FJsonValue>>* Rules = nullptr;
	if (TestTrue(TEXT("availableRules 应是数组"), J->TryGetArrayField(TEXT("availableRules"), Rules)))
	{
		TestEqual(TEXT("候选规则数量"), Rules->Num(), 1);
		TSharedPtr<FJsonObject> R = (*Rules)[0]->AsObject();
		// 日志里仍是三字段——conflictsWith 只进上行请求，不进日志，
		// 这样日志格式与重构前逐字节一致，不必动 schemaVersion 与前端 TS 镜像
		TestEqual(TEXT("日志的规则元素应为 tag/level/cost 三字段"), R->Values.Num(), 3);
		TestFalse(TEXT("日志不该带 conflictsWith（那是上行请求专有）"),
			R->HasField(TEXT("conflictsWith")));
		TestEqual(TEXT("tag 应写完整 Tag 名"),
			R->GetStringField(TEXT("tag")), FString(TEXT("Rule.Ammo")));
		TestEqual(TEXT("level"), R->GetStringField(TEXT("level")), FString(TEXT("medium")));
		TestEqual(TEXT("cost"),  R->GetNumberField(TEXT("cost")), 20.0);
	}
	return true;
}

// ---------------------------------------------------------------------------
// 上行请求：七个 Context 字段一个都不能少
//
// 少了 availableArchetypes 或 decisionHistory，服务端连"这条规则上一层用过没有"
// 都不知道，只能瞎选——选完必被客户端 Fairness 护栏拦下。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireRequestCompleteTest,
	"SHM.Director.Wire.Request_CarriesAllSevenContextFields", WireTestFlags)

bool FWireRequestCompleteTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> J =
		FSHMDirectorWire::BuildIntentRequest(WireMakeContext(), TEXT("20260730_143012_ab12"));
	if (!TestNotNull(TEXT("请求体不应为空"), J.Get())) { return false; }

	// 顶层第一个字段永远是 schemaVersion——格式演进时读取方据此决定怎么解析
	TestTrue(TEXT("必须有 schemaVersion"), J->HasField(TEXT("schemaVersion")));
	TestEqual(TEXT("schemaVersion 应为 1"), J->GetIntegerField(TEXT("schemaVersion")), 1);

	TestEqual(TEXT("runId 应原样带上"),
		J->GetStringField(TEXT("runId")), FString(TEXT("20260730_143012_ab12")));

	// FDirectorContext 的七个字段
	TestTrue(TEXT("① profile"),             J->HasField(TEXT("profile")));
	TestTrue(TEXT("② floorIndex"),          J->HasField(TEXT("floorIndex")));
	TestTrue(TEXT("③ totalFloors"),         J->HasField(TEXT("totalFloors")));
	TestTrue(TEXT("④ challengeBudget"),     J->HasField(TEXT("challengeBudget")));
	TestTrue(TEXT("⑤ availableRules"),      J->HasField(TEXT("availableRules")));
	TestTrue(TEXT("⑥ availableArchetypes"), J->HasField(TEXT("availableArchetypes")));
	TestTrue(TEXT("⑦ decisionHistory"),     J->HasField(TEXT("decisionHistory")));

	// 画像必须是完整的七维，不能只带 dominantArchetype 了事。
	// 请求里比日志多一个 primaryBuildTags —— prompt 的「主力打法」一行要用它，
	// 不发服务端就少一行 prompt，产出会与直连模式不一致。
	TSharedPtr<FJsonObject> P = J->GetObjectField(TEXT("profile"));
	if (P.IsValid())
	{
		TestEqual(TEXT("请求里的画像 = 共用七维 + primaryBuildTags"), P->Values.Num(), 8);
		TestTrue(TEXT("必须带 primaryBuildTags（prompt 要用）"),
			P->HasField(TEXT("primaryBuildTags")));
	}

	const TArray<TSharedPtr<FJsonValue>>* Archs = nullptr;
	if (TestTrue(TEXT("availableArchetypes 应是数组"),
		J->TryGetArrayField(TEXT("availableArchetypes"), Archs)))
	{
		TestEqual(TEXT("候选原型数量"), Archs->Num(), 2);
		TestEqual(TEXT("原型应写完整 Tag 名"),
			(*Archs)[0]->AsString(), FString(TEXT("Enemy.Grunt")));
	}

	// 请求里的规则必须带 conflictsWith —— prompt 要注入它。
	// 不给，LLM 只能盲选：实测 DeepSeek 同时挑了「弹药↓ + 远程伤害↓」，
	// 对远程玩家无解，被 Conflict 护栏拒并白白降级一次。
	const TArray<TSharedPtr<FJsonValue>>* ReqRules = nullptr;
	if (TestTrue(TEXT("availableRules 应是数组"),
		J->TryGetArrayField(TEXT("availableRules"), ReqRules)))
	{
		TSharedPtr<FJsonObject> R = (*ReqRules)[0]->AsObject();
		TestEqual(TEXT("请求的规则元素 = 共用三字段 + conflictsWith"), R->Values.Num(), 4);
		const TArray<TSharedPtr<FJsonValue>>* Conflicts = nullptr;
		if (TestTrue(TEXT("conflictsWith 应是数组"),
			R->TryGetArrayField(TEXT("conflictsWith"), Conflicts)))
		{
			TestEqual(TEXT("互斥项数量"), Conflicts->Num(), 1);
			TestEqual(TEXT("互斥项应写完整 Tag 名"),
				(*Conflicts)[0]->AsString(), FString(TEXT("Rule.RangedDamage")));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// 历史条目：只写结构体里真实存在的字段
//
// 设计文档 §5.1 的示例里画了 challengeLevel 与 playerAdapted，但
// FDirectorHistoryEntry 里没有这两项数据，也没有任何地方计算 playerAdapted。
// 写进去只能填常量 —— 那是把"产品算不出来的状态"当成记录发出去，
// 与踩坑 #23（四层日志）同一类问题。这条测试锁死这个决定。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireHistoryNoFabricationTest,
	"SHM.Director.Wire.Request_HistoryOmitsFieldsWithNoDataSource", WireTestFlags)

bool FWireHistoryNoFabricationTest::RunTest(const FString&)
{
	TSharedPtr<FJsonObject> J =
		FSHMDirectorWire::BuildIntentRequest(WireMakeContext(), TEXT("run"));
	if (!TestNotNull(TEXT("请求体不应为空"), J.Get())) { return false; }

	const TArray<TSharedPtr<FJsonValue>>* Hist = nullptr;
	if (!TestTrue(TEXT("decisionHistory 应是数组"),
		J->TryGetArrayField(TEXT("decisionHistory"), Hist))) { return false; }

	TestEqual(TEXT("历史条目数"), Hist->Num(), 1);
	TSharedPtr<FJsonObject> H = (*Hist)[0]->AsObject();
	if (!H.IsValid()) { return false; }

	TestTrue(TEXT("应有 floorIndex"), H->HasField(TEXT("floorIndex")));
	TestTrue(TEXT("应有 ruleTags"),   H->HasField(TEXT("ruleTags")));

	TestEqual(TEXT("历史条目恰好两个字段——结构体里只有这两项数据"), H->Values.Num(), 2);
	TestFalse(TEXT("不得凭空写 challengeLevel（结构体无此数据）"),
		H->HasField(TEXT("challengeLevel")));
	TestFalse(TEXT("不得凭空写 playerAdapted（无任何地方计算它）"),
		H->HasField(TEXT("playerAdapted")));

	return true;
}

// ---------------------------------------------------------------------------
// 踩坑 #22 的上行版回归：C++ 类型名不得泄漏进请求体
//
// 服务端是 Java。"ESHMGuardrail::Conflict" 这类全限定名它一样解析不出来。
// 这条测试扫的是**整个请求体的序列化文本**，不是某个字段——
// 因为下次泄漏未必发生在同一个字段上。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireNoCppTypeLeakTest,
	"SHM.Director.Wire.Request_LeaksNoCppTypeNames", WireTestFlags)

bool FWireNoCppTypeLeakTest::RunTest(const FString&)
{
	const FString Body =
		FSHMDirectorWire::BuildIntentRequestString(WireMakeContext(), TEXT("run"));

	if (!TestTrue(TEXT("请求体文本不应为空"), !Body.IsEmpty())) { return false; }

	TestFalse(TEXT("不得出现 UE 枚举类型前缀 ESHM"), Body.Contains(TEXT("ESHM")));
	TestFalse(TEXT("不得出现 E 打头的枚举全限定名 EChallengeLevel"),
		Body.Contains(TEXT("EChallengeLevel")));
	TestFalse(TEXT("不得出现 C++ 作用域解析符 ::（Tag 名用点分隔）"),
		Body.Contains(TEXT("::")));
	TestFalse(TEXT("不得出现 UE 的 None 占位（空 Tag 应被跳过而非写成 None）"),
		Body.Contains(TEXT("\"None\"")));

	// 反向确认：该有的完整 Tag 名确实在
	TestTrue(TEXT("应包含完整 Tag 名 Rule.Ammo"), Body.Contains(TEXT("Rule.Ammo")));
	TestTrue(TEXT("应包含完整 Tag 名 Archetype.Ranger"), Body.Contains(TEXT("Archetype.Ranger")));
	TestTrue(TEXT("应包含主力打法标签 Build.Ranged（prompt 要用）"),
		Body.Contains(TEXT("Build.Ranged")));

	return true;
}

// ---------------------------------------------------------------------------
// 单一真源：日志与请求里的画像块必须由同一个函数产出
//
// 这是本次重构存在的全部理由。若哪天有人给请求体的画像"顺手加个字段"，
// 而日志那边没加，这条会红。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWireProfileSingleSourceTest,
	"SHM.Director.Wire.Profile_SameShapeInLogAndRequest", WireTestFlags)

bool FWireProfileSingleSourceTest::RunTest(const FString&)
{
	const FPlayerProfile Profile = WireMakeProfile();

	TSharedPtr<FJsonObject> Direct = FSHMDirectorWire::ProfileToJson(Profile);

	FDirectorContext Ctx = WireMakeContext();
	Ctx.Profile = Profile;
	TSharedPtr<FJsonObject> Req = FSHMDirectorWire::BuildIntentRequest(Ctx, TEXT("run"));
	TSharedPtr<FJsonObject> InReq = Req.IsValid() ? Req->GetObjectField(TEXT("profile")) : nullptr;

	if (!TestNotNull(TEXT("直接序列化的画像"), Direct.Get())) { return false; }
	if (!TestNotNull(TEXT("请求体里的画像"), InReq.Get())) { return false; }

	// ⚠️ 只比"两边字段数相同"是不够的：两边都空时 0 == 0 也成立，
	// 桩实现能骗过这条断言。先钉死字段数不为空，这条测试才有牙。
	TestEqual(TEXT("共用画像应有七个字段（空对象不算通过）"), Direct->Values.Num(), 7);

	// 关系是**包含**而非相等：请求体额外带 primaryBuildTags（prompt 要用），
	// 日志不带（保持与重构前逐字节一致）。两份契约允许有各自的字段，
	// 但**共用的那七维必须一字不差** —— 这才是"单一真源"要守的东西。
	for (const auto& Pair : Direct->Values)
	{
		TestTrue(FString::Printf(TEXT("请求体画像应含共用字段 %s"), *Pair.Key),
			InReq->HasField(Pair.Key));
	}
	TestTrue(TEXT("请求体画像不得少于共用七维"), InReq->Values.Num() >= Direct->Values.Num());
	return true;
}
