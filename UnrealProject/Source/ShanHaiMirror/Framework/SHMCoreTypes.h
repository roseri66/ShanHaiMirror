#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SHMCoreTypes.generated.h"

// ============================================================================
// 攻击模式枚举 —— AI Director 通过 WeaponTag 识别打法，不读具体武器名
// ============================================================================
UENUM(BlueprintType)
enum class EAttackPattern : uint8
{
	Melee_Sweep   UMETA(DisplayName = "近战扇形横扫"),
	Melee_Heavy   UMETA(DisplayName = "近战重击"),
	Ranged_Shot   UMETA(DisplayName = "远程单发"),
	Ranged_Pierce UMETA(DisplayName = "远程穿透 AOE"),
};

// ============================================================================
// 敌人原型 —— AI Director 只按原型标签调权重，不指定具体怪
// ============================================================================
UENUM(BlueprintType)
enum class EEnemyArchetype : uint8
{
	Grunt      UMETA(DisplayName = "杂兵"),
	Tank       UMETA(DisplayName = "盾兵"),
	Rush       UMETA(DisplayName = "突进"),
	Shooter    UMETA(DisplayName = "射手"),
	Controller UMETA(DisplayName = "控制"),  // S5
	Support    UMETA(DisplayName = "辅助"),  // S5
	Elite      UMETA(DisplayName = "精英"),
	Boss       UMETA(DisplayName = "Boss"),
};

// ============================================================================
// 挑战等级 —— LocalDirectorCore 决策输出
// ============================================================================
UENUM(BlueprintType)
enum class EChallengeLevel : uint8
{
	Recovery  = 0 UMETA(DisplayName = "恢复期(降压力)"),
	Stable    = 1 UMETA(DisplayName = "正常"),
	Pressure  = 2 UMETA(DisplayName = "施压"),
	Counter   = 3 UMETA(DisplayName = "明确反制"),
	Evolution = 4 UMETA(DisplayName = "强制转型"),
};

// ============================================================================
// 规则修改器 —— 一条规则 = 标签 + 等级（具体数值由 DT_Rule 查表）
// ============================================================================
USTRUCT(BlueprintType)
struct FRuleModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag RuleTag;    // Rule.Ammo / Rule.Heal / Rule.Cooldown ...

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Level = "medium"; // light / medium / heavy

	// 已解析的生效数值（RuleResolver 查 DT_Rule 产生，链路第 ⑥ 步）。
	// 这是全链路里数值第一次出现的地方——Provider 的 FDirectorIntent 里没有它。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Multiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Reason;          // 调试用：为什么选这条规则
};

// ============================================================================
// AI Director 的完整决策输出 —— 这是 AI 层与玩法层的唯一接口
// ============================================================================
USTRUCT(BlueprintType)
struct FDirectorDecision
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EChallengeLevel ChallengeLevel = EChallengeLevel::Stable;

	// 敌人原型 → 权重（如 Enemy.Tank → 0.3）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, float> EnemyWeights;

	// 选中的规则列表
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FRuleModifier> RuleModifiers;

	// 资源调整（Resource.Heal → -0.15 表示回复道具减少 15%）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, float> ResourceAdjust;

	// 战场偏好标签
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag ArenaPreference;

	// 白泽台词
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString NarrationLine;

	// 内部调试用
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Reason;
};

// ============================================================================
// 每层行为快照 —— BehaviorRecorder 累积，层结束时定稿
// ============================================================================
USTRUCT(BlueprintType)
struct FFloorBehaviorSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 FloorIndex = 0;

	// 维度1：Build 集中度
	UPROPERTY(BlueprintReadOnly)
	TMap<FGameplayTag, int32> WeaponAttackCounts;

	UPROPERTY(BlueprintReadOnly)
	TMap<FGameplayTag, int32> SkillCastCounts;

	UPROPERTY(BlueprintReadOnly)
	TMap<FGameplayTag, float> DamageByTag;

	UPROPERTY(BlueprintReadOnly)
	TMap<FGameplayTag, int32> KillsByTag;

	// 维度2：战斗效率
	// 注意这里存的是**原始计数**而不是比率。比率会丢掉样本量：
	// 闪避 2 次成功 1 次 与 闪避 40 次成功 20 次 都是 50%，但前者毫无统计意义。
	// ProfileAnalyzer 需要靠样本量决定置信度，所以除法留给它做。
	UPROPERTY(BlueprintReadOnly)
	int32 RoomsCleared = 0;

	UPROPERTY(BlueprintReadOnly)
	float TotalRoomClearTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 HitsTaken = 0;

	UPROPERTY(BlueprintReadOnly)
	float TotalDamageTaken = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 DodgeAttempts = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DodgeSuccesses = 0;

	// 维度3：资源盈余
	UPROPERTY(BlueprintReadOnly)
	int32 HealItemsRemaining = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 AmmoRemaining = 0;

	// 维度4：策略切换意愿
	UPROPERTY(BlueprintReadOnly)
	int32 WeaponSwitchCount = 0;

	UPROPERTY(BlueprintReadOnly)
	TSet<FGameplayTag> UniqueSkillsUsed;

	// 本层用过的不同武器（「策略切换意愿」的另一个信号：只用一把 vs 来回换）
	UPROPERTY(BlueprintReadOnly)
	TSet<FGameplayTag> UniqueWeaponsUsed;

	// 原 BuildDiffFromLastFloor 已移除：它是「与上一层比较」的派生量，
	// 属于分析而非观察。ProfileAnalyzer 拿得到 History，自己算即可。

	// 维度5：生存压力
	UPROPERTY(BlueprintReadOnly)
	int32 LowHpEvents = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DeathOrNearDeath = 0;
};

// ============================================================================
// 玩家画像 —— ProfileAnalyzer 输出，纯确定性算法
// ============================================================================
USTRUCT(BlueprintType)
struct FPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float BuildConcentration = 0.f;    // 0-100

	UPROPERTY(BlueprintReadOnly)
	float CombatEfficiency = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ResourceSurplus = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float StrategySwitch = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float SurvivalPressure = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag DominantArchetype;    // Archetype.Ranger / Vanguard / Berserker / Survivor

	UPROPERTY(BlueprintReadOnly)
	TArray<FGameplayTag> PrimaryBuildTags;

	UPROPERTY(BlueprintReadOnly)
	float Confidence = 0.5f;           // 0-1，连续 N 层同打法才升高
};
