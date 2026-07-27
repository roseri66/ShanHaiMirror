#include "SHMJsonIntent.h"
#include "SHMDecisionLogFormat.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

using namespace SHMLogFormat;

// ============================================================================
//  挑战等级 ↔ 字符串（日志可读性优先；未知串一律回落 Stable，不抛不崩）
// ============================================================================
FString FSHMJsonIntent::ChallengeLevelToString(EChallengeLevel Level)
{
	switch (Level)
	{
	case EChallengeLevel::Recovery:  return TEXT("Recovery");
	case EChallengeLevel::Pressure:  return TEXT("Pressure");
	case EChallengeLevel::Counter:   return TEXT("Counter");
	case EChallengeLevel::Evolution: return TEXT("Evolution");
	default:                         return TEXT("Stable");
	}
}

EChallengeLevel FSHMJsonIntent::ChallengeLevelFromString(const FString& Str)
{
	if (Str.Equals(TEXT("Recovery"),  ESearchCase::IgnoreCase)) { return EChallengeLevel::Recovery;  }
	if (Str.Equals(TEXT("Pressure"),  ESearchCase::IgnoreCase)) { return EChallengeLevel::Pressure;  }
	if (Str.Equals(TEXT("Counter"),   ESearchCase::IgnoreCase)) { return EChallengeLevel::Counter;   }
	if (Str.Equals(TEXT("Evolution"), ESearchCase::IgnoreCase)) { return EChallengeLevel::Evolution; }
	return EChallengeLevel::Stable;   // 含未知值：回落到最保守的等级
}

// ============================================================================
//  JSON 文本 → Intent
// ============================================================================
FDirectorIntent FSHMJsonIntent::ParseFromJson(const FString& Json, bool& bOutOk)
{
	bOutOk = false;

	if (Json.IsEmpty())
	{
		return FDirectorIntent();
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	// Deserialize 对"合法 JSON 但顶层不是对象"（数组/null/裸值）也会失败或给出空对象，
	// 两种情况都在这里被挡住
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return FDirectorIntent();
	}

	return ParseFromJsonObject(Root, bOutOk);
}

// ============================================================================
//  JSON 对象 → Intent
//
//  取值一律用 Try* 系列：字段缺失/类型不符时安全跳过，不产生错误日志洪水。
//  **不做业务判断**（标签在不在白名单、权重和是不是 1）——那是护栏的职责。
//  这里只保证"结构安全"：能解析的解析，不能解析的丢弃，绝不崩。
// ============================================================================
FDirectorIntent FSHMJsonIntent::ParseFromJsonObject(const TSharedPtr<FJsonObject>& Obj, bool& bOutOk)
{
	bOutOk = false;
	FDirectorIntent Intent;

	if (!Obj.IsValid())
	{
		return Intent;
	}

	// --- 挑战等级（缺失 → Stable）---
	FString LevelStr;
	if (Obj->TryGetStringField(Key_ChallengeLevel, LevelStr))
	{
		Intent.ChallengeLevel = ChallengeLevelFromString(LevelStr);
	}

	// --- 敌人权重：Intent 可用性的最低要求 ---
	const TSharedPtr<FJsonObject>* WeightsObj = nullptr;
	if (Obj->TryGetObjectField(Key_EnemyWeights, WeightsObj) && WeightsObj && WeightsObj->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*WeightsObj)->Values)
		{
			double Weight = 0.0;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Weight))
			{
				continue;   // 权重不是数字：跳过这一条，其余照常
			}

			// 幻觉标签（未注册）会得到无效 Tag——照常放行，由 Schema 护栏白名单拒绝。
			// 解析器不越权做业务判断。
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Pair.Key), /*ErrorIfNotFound=*/false);
			if (Tag.IsValid())
			{
				Intent.EnemyWeights.Add(Tag, static_cast<float>(Weight));
			}
		}
	}

	// --- 规则意图：只取 tag + level。**任何数值字段（multiplier/cost）在此蒸发** ---
	const TArray<TSharedPtr<FJsonValue>>* RulesArray = nullptr;
	if (Obj->TryGetArrayField(Key_RuleIntents, RulesArray) && RulesArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *RulesArray)
		{
			const TSharedPtr<FJsonObject>* RuleObj = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(RuleObj) || !RuleObj || !RuleObj->IsValid())
			{
				continue;
			}

			FString TagStr, LevelField;
			if (!(*RuleObj)->TryGetStringField(Key_Tag, TagStr))
			{
				continue;   // 无标签的规则无意义
			}
			const FGameplayTag RuleTag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
			if (!RuleTag.IsValid())
			{
				continue;   // 幻觉规则标签：直接丢（它连查表都查不到）
			}

			FRuleIntent Rule;
			Rule.RuleTag = RuleTag;
			Rule.Level   = (*RuleObj)->TryGetStringField(Key_Level, LevelField) ? LevelField : TEXT("light");
			Intent.RuleIntents.Add(Rule);
		}
	}

	// --- 文本字段 ---
	Obj->TryGetStringField(Key_Narration, Intent.Narration);
	Obj->TryGetStringField(Key_Reason,    Intent.Reason);

	// 可用性底线：没有任何敌人权重的 Intent 无法驱动刷怪，判不可用让调用方降级。
	// （权重非法/越界不在这里判——那是 Schema 护栏的活）
	bOutOk = Intent.EnemyWeights.Num() > 0;
	return Intent;
}

// ============================================================================
//  Intent → JSON 对象（决策日志的 rawIntent；与解析严格对称，保证往返一致）
// ============================================================================
TSharedPtr<FJsonObject> FSHMJsonIntent::ToJsonObject(const FDirectorIntent& Intent)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	Obj->SetStringField(Key_ChallengeLevel, ChallengeLevelToString(Intent.ChallengeLevel));

	TSharedPtr<FJsonObject> Weights = MakeShared<FJsonObject>();
	for (const TPair<FGameplayTag, float>& Pair : Intent.EnemyWeights)
	{
		Weights->SetNumberField(Pair.Key.GetTagName().ToString(), Pair.Value);
	}
	Obj->SetObjectField(Key_EnemyWeights, Weights);

	TArray<TSharedPtr<FJsonValue>> Rules;
	for (const FRuleIntent& Rule : Intent.RuleIntents)
	{
		TSharedPtr<FJsonObject> RuleObj = MakeShared<FJsonObject>();
		RuleObj->SetStringField(Key_Tag,   Rule.RuleTag.GetTagName().ToString());
		RuleObj->SetStringField(Key_Level, Rule.Level);
		// 此处刻意不写任何数值——rawIntent 就该是"护栏前、查表前"的原貌
		Rules.Add(MakeShared<FJsonValueObject>(RuleObj));
	}
	Obj->SetArrayField(Key_RuleIntents, Rules);

	Obj->SetStringField(Key_Narration, Intent.Narration);
	Obj->SetStringField(Key_Reason,    Intent.Reason);

	return Obj;
}
