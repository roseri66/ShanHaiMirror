#include "SHMPromptBuilder.h"
#include "SHMJsonIntent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// ============================================================================
//  System prompt —— 角色 + 严格输出契约
//
//  三条硬约束按重要性排序写死在这里：
//    ① 只输出 JSON（不要解释、不要 markdown 代码围栏）
//    ② 只能从给定候选集里选（越界会被护栏拒，白白浪费一次调用）
//    ③ **禁止任何数值**——D-15 在自然语言层的表述
// ============================================================================
FString FSHMPromptBuilder::BuildSystemPrompt()
{
	return TEXT(
		"你是《山海镜》的 AI 导演「白泽」，一只通晓万物的神兽。"
		"你的职责：读玩家的战斗画像，为下一层挑选敌人配比与规则调整，并用一句话向玩家解释。\n"
		"\n"
		"【输出格式】只输出一个 JSON 对象，不要任何解释文字，不要 markdown 代码围栏。字段：\n"
		"  challengeLevel : 字符串，取值 Recovery / Stable / Pressure / Counter 之一\n"
		"  enemyWeights   : 对象，敌人原型标签 → 权重(0~1)，所有权重之和必须等于 1\n"
		"  ruleIntents    : 数组，每项为 { \"tag\": 规则标签, \"level\": light|medium }\n"
		"  narration      : 字符串，白泽对玩家说的一句话（中文，有性格，不超过 40 字）\n"
		"  reason         : 字符串，你这样决策的理由（中文，给开发者看）\n"
		"\n"
		"【硬性约束】\n"
		"1. enemyWeights 的键只能来自「可用敌人原型」列表；ruleIntents 的 (tag, level) 组合"
		"只能来自「可用规则」列表。列表之外的一律不许出现——凭空编造的标签会被系统拒绝。\n"
		"2. 所选规则的 cost 之和不得超过「挑战预算」。\n"
		"3. **禁止输出任何数值型的规则强度**：不要 multiplier、不要百分比、不要伤害数字。"
		"你只负责选「哪条规则、什么等级」，具体数值由游戏系统查表决定。擅自给数值会被丢弃。\n"
		"4. 你的目标不是把玩家玩死，而是逼他改变打法。玩家挣扎时要收手，顺风时才施压。\n"
		"\n"
		"【输出样例】（格式示意，标签请按实际候选集选）\n"
		"{\"challengeLevel\":\"Counter\","
		"\"enemyWeights\":{\"Enemy.Grunt\":0.4,\"Enemy.Tank\":0.35,\"Enemy.Rush\":0.25},"
		"\"ruleIntents\":[{\"tag\":\"Rule.Ammo\",\"level\":\"medium\"}],"
		"\"narration\":\"你的弓用得很好。但这一层，别指望站在原地。\","
		"\"reason\":\"高置信度反制远程站桩。\"}\n"
	);
}

// ============================================================================
//  User prompt —— 注入画像与候选集
//
//  注入的候选集就是 Context 里那个已过滤的安全集：
//  不给越界选项（第一层约束），护栏再兜底（第二层）。
// ============================================================================
FString FSHMPromptBuilder::BuildUserPrompt(const FDirectorContext& Context)
{
	FString Out;

	Out += FString::Printf(TEXT("【当前进度】第 %d 层 / 共 %d 层\n"),
		Context.FloorIndex + 1, Context.TotalFloors);

	const FPlayerProfile& P = Context.Profile;
	Out += TEXT("\n【玩家画像】（0-100）\n");
	Out += FString::Printf(TEXT("  Build 集中度 : %.0f（越高越依赖单一打法）\n"), P.BuildConcentration);
	Out += FString::Printf(TEXT("  战斗效率     : %.0f\n"), P.CombatEfficiency);
	Out += FString::Printf(TEXT("  策略切换意愿 : %.0f（越低越像「一招鲜」）\n"), P.StrategySwitch);
	Out += FString::Printf(TEXT("  生存压力     : %.0f（越高说明玩家越吃力）\n"), P.SurvivalPressure);
	Out += FString::Printf(TEXT("  判断置信度   : %.2f（低于 0.6 时不要激进针对）\n"), P.Confidence);

	if (P.DominantArchetype.IsValid())
	{
		Out += FString::Printf(TEXT("  主导原型     : %s\n"), *P.DominantArchetype.GetTagName().ToString());
	}
	if (P.PrimaryBuildTags.Num() > 0)
	{
		Out += TEXT("  主力打法     :");
		for (const FGameplayTag& Tag : P.PrimaryBuildTags)
		{
			Out += FString::Printf(TEXT(" %s"), *Tag.GetTagName().ToString());
		}
		Out += TEXT("\n");
	}

	Out += FString::Printf(TEXT("\n【挑战预算】%d（所选规则 cost 之和不得超过它）\n"), Context.ChallengeBudget);

	Out += TEXT("\n【可用敌人原型】（enemyWeights 的键只能从这里选）\n");
	for (const FGameplayTag& Tag : Context.AvailableArchetypes)
	{
		Out += FString::Printf(TEXT("  %s\n"), *Tag.GetTagName().ToString());
	}

	Out += TEXT("\n【可用规则】（ruleIntents 只能从这些 (tag, level) 组合里选）\n");
	if (Context.AvailableRules.Num() == 0)
	{
		Out += TEXT("  （本层无可用规则，ruleIntents 请返回空数组）\n");
	}
	for (const FSHMAvailableRule& Rule : Context.AvailableRules)
	{
		Out += FString::Printf(TEXT("  tag=%s level=%s cost=%d"),
			*Rule.RuleTag.GetTagName().ToString(), *Rule.Level, Rule.Cost);

		// 互斥信息必须一并注入。不给它，LLM 只能盲选，
		// 实测 DeepSeek 会同时挑「弹药↓ + 远程伤害↓」——对远程玩家是无解组合，
		// 护栏会拒、然后白白降级一次。候选集要给全，才叫「只给安全选项」。
		if (!Rule.ConflictsWith.IsEmpty())
		{
			Out += TEXT("  【不可与以下规则同时选用：");
			for (const FGameplayTag& Conflict : Rule.ConflictsWith)
			{
				Out += FString::Printf(TEXT(" %s"), *Conflict.GetTagName().ToString());
			}
			Out += TEXT("】");
		}
		Out += TEXT("\n");
	}

	// 历史：让 LLM 知道上层做过什么，避免连续针对同一点（Fairness 护栏也会拦，
	// 但事先告知能减少无谓的降级）
	if (Context.DecisionHistory.Num() > 0)
	{
		Out += TEXT("\n【最近几层已用过的规则】（避免连续重复针对）\n");
		for (const FDirectorHistoryEntry& Entry : Context.DecisionHistory)
		{
			Out += FString::Printf(TEXT("  第 %d 层:"), Entry.FloorIndex + 1);
			if (Entry.AppliedRuleTags.Num() == 0)
			{
				Out += TEXT(" （无）");
			}
			for (const FGameplayTag& Tag : Entry.AppliedRuleTags)
			{
				Out += FString::Printf(TEXT(" %s"), *Tag.GetTagName().ToString());
			}
			Out += TEXT("\n");
		}
	}

	Out += TEXT("\n请给出这一层的导演决策 JSON。");
	return Out;
}

// ============================================================================
//  完整请求体（OpenAI 兼容 chat completions）
// ============================================================================
FString FSHMPromptBuilder::BuildRequestBody(const FDirectorContext& Context, const FString& Model)
{
	const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), Model);

	auto MakeMessage = [](const TCHAR* Role, const FString& Content)
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), Role);
		Msg->SetStringField(TEXT("content"), Content);
		return MakeShared<FJsonValueObject>(Msg);
	};

	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(MakeMessage(TEXT("system"), BuildSystemPrompt()));
	Messages.Add(MakeMessage(TEXT("user"),   BuildUserPrompt(Context)));
	Root->SetArrayField(TEXT("messages"), Messages);

	// 温度略低：要的是"有品味的选择"，不是天马行空。太高会显著提高非法输出率。
	Root->SetNumberField(TEXT("temperature"), 0.7);

	// 要求 JSON 输出（OpenAI 兼容端点普遍支持；不支持的端点会忽略此字段，
	// 届时靠 system prompt 的约束 + 解析器容错兜底）
	const TSharedPtr<FJsonObject> Format = MakeShared<FJsonObject>();
	Format->SetStringField(TEXT("type"), TEXT("json_object"));
	Root->SetObjectField(TEXT("response_format"), Format);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Body;
}
