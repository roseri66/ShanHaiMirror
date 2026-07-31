#include "SHMDirectorWireFormat.h"
#include "SHMDecisionLogFormat.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	// 标签 → 完整 Tag 名（"Rule.Ammo"）。
	//
	// **不用 ToString() / GetValueAsString() / Printf("%s")**——那些是给人看的调试输出，
	// 格式随时可能变，也不承诺跨语言可解析。踩坑 #22 就是这么来的。
	// 空标签返回空串而不是 "None"：UE 的 FGameplayTag 无效时 GetTagName() 给的是
	// NAME_None，序列化出来就是字面量 "None"，服务端会把它当成一个真实的标签名。
	FString WireTagToString(const FGameplayTag& Tag)
	{
		return Tag.IsValid() ? Tag.GetTagName().ToString() : FString();
	}
}

TSharedPtr<FJsonObject> FSHMDirectorWire::ProfileToJson(const FPlayerProfile& Profile)
{
	using namespace SHMWireFormat;

	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(Key_BuildConcentration, Profile.BuildConcentration);
	J->SetNumberField(Key_CombatEfficiency,   Profile.CombatEfficiency);
	J->SetNumberField(Key_ResourceSurplus,    Profile.ResourceSurplus);
	J->SetNumberField(Key_StrategySwitch,     Profile.StrategySwitch);
	J->SetNumberField(Key_SurvivalPressure,   Profile.SurvivalPressure);
	// Confidence 是 0-1 量纲，其余五维是 0-100。刻意不做归一化——
	// 契约里量纲不同是已知的，改成统一量纲要 +SchemaVersion 并同步改前端与服务端。
	J->SetNumberField(Key_Confidence,         Profile.Confidence);
	J->SetStringField(Key_DominantArchetype,  WireTagToString(Profile.DominantArchetype));

	// PrimaryBuildTags 刻意不进契约：它是 ProfileAnalyzer 的中间产物，
	// 决策不消费它（本地 Provider 与 prompt 都只看 DominantArchetype）。
	// 进契约就等于承诺维护它，而它现在没有消费方。
	return J;
}

TSharedPtr<FJsonObject> FSHMDirectorWire::AvailableRuleToJson(const FSHMAvailableRule& Rule)
{
	// tag / level / cost 三个 key 直接引用日志契约的常量：候选规则在两份契约里
	// 本来就是同一个形状，各定义一份等于给同一个东西造两个真源。
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(SHMLogFormat::Key_Tag,   WireTagToString(Rule.RuleTag));
	J->SetStringField(SHMLogFormat::Key_Level, Rule.Level);
	J->SetNumberField(SHMLogFormat::Key_Cost,  Rule.Cost);

	// ConflictsWith 不进契约：互斥判定由**客户端** Conflict 护栏做（D-23 明确否决
	// 把护栏搬到服务端）。服务端拿到它也不该用它做拒绝，给了反而是误导。
	// prompt 里需要的互斥信息由服务端从自己的规则配置取。
	return J;
}

TSharedPtr<FJsonObject> FSHMDirectorWire::BuildIntentRequest(const FDirectorContext& Context,
	const FString& RunId)
{
	using namespace SHMWireFormat;

	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();

	// 顶层第一个字段永远是 schemaVersion
	J->SetNumberField(Key_SchemaVersion, SchemaVersion);
	J->SetStringField(Key_RunId, RunId);

	// FDirectorContext 的七个字段，一个不少 ——
	// 少 availableArchetypes 或 decisionHistory，服务端只能瞎选，
	// 选完必被客户端 Fairness 护栏拦下。
	J->SetObjectField(Key_Profile, ProfileToJson(Context.Profile));
	J->SetNumberField(Key_FloorIndex, Context.FloorIndex);
	J->SetNumberField(Key_TotalFloors, Context.TotalFloors);
	J->SetNumberField(Key_ChallengeBudget, Context.ChallengeBudget);

	TArray<TSharedPtr<FJsonValue>> Rules;
	for (const FSHMAvailableRule& Rule : Context.AvailableRules)
	{
		Rules.Add(MakeShared<FJsonValueObject>(AvailableRuleToJson(Rule)));
	}
	J->SetArrayField(Key_AvailableRules, Rules);

	TArray<TSharedPtr<FJsonValue>> Archetypes;
	for (const FGameplayTag& Tag : Context.AvailableArchetypes)
	{
		if (!Tag.IsValid()) { continue; }   // 无效标签跳过，不写成 "None"
		Archetypes.Add(MakeShared<FJsonValueString>(WireTagToString(Tag)));
	}
	J->SetArrayField(Key_AvailableArchetypes, Archetypes);

	TArray<TSharedPtr<FJsonValue>> History;
	for (const FDirectorHistoryEntry& Entry : Context.DecisionHistory)
	{
		TSharedPtr<FJsonObject> H = MakeShared<FJsonObject>();
		H->SetNumberField(Key_FloorIndex, Entry.FloorIndex);

		TArray<TSharedPtr<FJsonValue>> Tags;
		for (const FGameplayTag& Tag : Entry.AppliedRuleTags)
		{
			if (!Tag.IsValid()) { continue; }
			Tags.Add(MakeShared<FJsonValueString>(WireTagToString(Tag)));
		}
		H->SetArrayField(Key_RuleTags, Tags);

		// ⚠️ 只有这两个字段。设计文档 §5.1 的示例里还有 challengeLevel 与
		// playerAdapted，但 FDirectorHistoryEntry 里没有这两项数据，
		// 也没有任何地方计算 playerAdapted —— 写进去只能填常量，
		// 那就是把"产品算不出来的状态"当成记录发出去（踩坑 #23 同类）。
		History.Add(MakeShared<FJsonValueObject>(H));
	}
	J->SetArrayField(Key_DecisionHistory, History);

	return J;
}

FString FSHMDirectorWire::BuildIntentRequestString(const FDirectorContext& Context,
	const FString& RunId)
{
	TSharedPtr<FJsonObject> J = BuildIntentRequest(Context, RunId);

	FString Out;
	// Condensed：请求体不需要给人读，省带宽。日志导出那边才用 Pretty。
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(J.ToSharedRef(), Writer);
	return Out;
}

TSharedPtr<FJsonObject> FSHMDirectorWire::LogContextToJson(const FDirectorContext& Context)
{
	// 决策日志的 context 块 = { challengeBudget, availableRules }。
	// **刻意只有两个字段**，与重构前 RecordLogEntry 的行为逐字段一致：
	// 日志记的是"当时允许挑什么"，完整输入以 profile / floorIndex 等形式
	// 存在同级，不重复。形状与上行请求不同构，这是设计如此。
	TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetNumberField(SHMWireFormat::Key_ChallengeBudget, Context.ChallengeBudget);

	TArray<TSharedPtr<FJsonValue>> Rules;
	for (const FSHMAvailableRule& Rule : Context.AvailableRules)
	{
		Rules.Add(MakeShared<FJsonValueObject>(AvailableRuleToJson(Rule)));
	}
	J->SetArrayField(SHMWireFormat::Key_AvailableRules, Rules);

	return J;
}
