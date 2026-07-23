#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Framework/SHMCoreTypes.h"
#include "SHMDirectorTypes.generated.h"

// ============================================================================
// 导演层内部类型
//
// 分层说明：FDirectorDecision（玩法层消费的唯一接口）在 Framework/SHMCoreTypes.h；
// 本文件的类型只在 Director 层内部与 Provider 之间流动，玩法层永远不 include 它。
// ============================================================================

// ----------------------------------------------------------------------------
// 规则意图 —— (标签, 等级)，**没有数值**
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FRuleIntent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag RuleTag;              // Rule.Ammo / Rule.Cooldown ...

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Level = TEXT("light");     // light / medium / heavy
};

// ----------------------------------------------------------------------------
// FDirectorIntent —— Provider（本地表 / LLM / 回放）的输出。
//
// ★ 本结构体的核心不变量：**没有任何"规则数值"字段。**
//   规则只以 (标签, 等级) 存在；×0.75 这类数值由 RuleResolver 查 DT_Rule 产生。
//   Provider（包括未来的 LLM）在编译期就构造不出带数值的决策——
//   "LLM 不接触数值"由类型系统强制，不靠纪律。（DECISIONS D-15）
//
//   EnemyWeights 是"配比"（和为 1 的权重分布），不是伤害/血量数值，允许存在。
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FDirectorIntent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChallengeLevel ChallengeLevel = EChallengeLevel::Stable;

	// 敌人原型 → 配比权重，应和为 1（Schema 护栏校验）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, float> EnemyWeights;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRuleIntent> RuleIntents;

	// 白泽台词（W4 起由 LLM 生成，本地 Provider 用模板句）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Narration;

	// 决策理由，进日志
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Reason;
};

// ----------------------------------------------------------------------------
// 候选规则 —— Context 里"允许 Provider 挑选"的一条规则。
// Cost / ConflictsWith 从 DT_Rule 拷贝而来，使 Validator 成为
// (Intent, Context) 的纯函数，校验时不需要再碰数据表。
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FSHMAvailableRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag RuleTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Level = TEXT("light");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer ConflictsWith;
};

// ----------------------------------------------------------------------------
// 历史决策记录 —— Fairness 护栏（同一规则不连续 N 层）与 W5 决策日志的数据源
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FDirectorHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FloorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGameplayTag> AppliedRuleTags;
};

// ----------------------------------------------------------------------------
// FDirectorContext —— 给 Provider 的输入，链路第 ③ 步 CONSTRAIN 的产物。
//
// 约束在这里收敛：AvailableRules 构建时就已剔除明显违规项（如连续使用中的规则），
// Provider 拿到的候选集本身是安全的。即便如此第 ⑤ 步仍全量校验——
// 不信任 Provider 输出是设计前提，不是补丁。
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FDirectorContext
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPlayerProfile Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FloorIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TotalFloors = 3;

	// 挑战预算：本层规则 Cost 之和的上限，按层深递增
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ChallengeBudget = 0;

	// 允许挑选的规则（已含 Cost / 冲突信息）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSHMAvailableRule> AvailableRules;

	// 允许调权的敌人原型（MVP 四原型）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FGameplayTag> AvailableArchetypes;

	// 往层的导演决策（Fairness 用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDirectorHistoryEntry> DecisionHistory;
};

// ----------------------------------------------------------------------------
// DT_Rule 行结构 —— (RuleTag, Level) → 数值与代价。
// 数据源为 Content/Data/RuleTable.csv（纯文本入库，可 diff）。
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FSHMRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag RuleTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Level = TEXT("light");

	// 生效数值：如弹药掉落 ×0.75。语义由具体规则的消费方（W3 FloorGenerator）解释
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Multiplier = 1.f;

	// 预算代价
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cost = 0;

	// 与哪些规则互斥（如 弹药↓ 与 远程伤害↓ 同时出现会把远程 Build 逼入无解）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer ConflictsWith;
};

// ----------------------------------------------------------------------------
// 校验结果
// ----------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FValidationResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bValid = true;

	// 拒绝原因（进日志；一次校验可能有多条违规，全部列出便于排查）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> RejectReasons;
};
