#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Framework/SHMCoreTypes.h"

// ============================================================================
// 画像分析器 —— AI Director 链路第 ② 步 ANALYZE
//
// **纯函数。这是全项目最该被单测覆盖的一个类。**
//
// 硬约束（破坏其中任何一条，可测性和断网可玩性同时失效）：
//   ✗ 不持有状态、不继承 UObject
//   ✗ 不碰 GWorld / 不碰随机数 / 不碰当前时间
//   ✗ 不读 DataTable、不发网络请求
//   ✓ 相同输入必定得到相同输出
//
// 契约：History 只含**已定稿的往层，不含 Current**。
//       （若把 Current 也塞进 History，"连续 N 层同打法"会把当前层算两次）
// ============================================================================
class SHANHAIMIRROR_API FSHMProfileAnalyzer
{
public:
	// 主入口：快照 + 历史 → 五维画像
	static FPlayerProfile Analyze(const FFloorBehaviorSnapshot& Current,
	                              const TArray<FFloorBehaviorSnapshot>& History);

	// --- 以下为可单独测试的子步骤（公开是为了让测试能精确定位失败点）---

	// Build 集中度：最大单武器攻击占比 × 100。零攻击返回 0。
	static float ComputeBuildConcentration(const FFloorBehaviorSnapshot& Snapshot);

	// 主力打法标签：集中度 > 阈值时，返回该武器对应的 Build 标签
	static TArray<FGameplayTag> ComputePrimaryBuildTags(const FFloorBehaviorSnapshot& Snapshot);

	// 武器标签 → Build 流派标签（MVP 硬编码；后续改为读 DT_Weapon.BuildTags）
	static FGameplayTag GetBuildTagForWeapon(const FGameplayTag& WeaponTag);

	// 单层的主导原型（不含跨层判断）
	static FGameplayTag ComputeDominantArchetype(const FFloorBehaviorSnapshot& Snapshot);

	// 置信度：连续同原型才升高，换打法则重置
	static float ComputeConfidence(const FFloorBehaviorSnapshot& Current,
	                               const TArray<FFloorBehaviorSnapshot>& History);

	// ---------------------------------------------------------------------
	// 其余四维
	//
	// 「没数据时返回什么」的原则（不统一会让导演读到假信号）：
	//   比率型维度（需要分母：房间数 / 攻击数）→ 无分母时返回中性 50，
	//       因为"测不出来"不等于"表现差"，返回 0 会让导演误判玩家在挣扎。
	//   计数型维度（生存压力）→ 无事件时返回 0，
	//       因为"没触发过残血"本身就是有效观测，不是缺数据。
	// ---------------------------------------------------------------------

	// 战斗效率：清怪快 + 挨打少 = 高。无已清房间时返回中性值。
	static float ComputeCombatEfficiency(const FFloorBehaviorSnapshot& Snapshot);

	// 资源盈余：当前 scope 内**无数据源**，恒返回中性值。见 .cpp 内说明。
	static float ComputeResourceSurplus(const FFloorBehaviorSnapshot& Snapshot);

	// 策略切换意愿：换武器频率 + 用过的武器种类。无任何武器使用时返回中性值。
	static float ComputeStrategySwitch(const FFloorBehaviorSnapshot& Snapshot);

	// 生存压力：残血次数 + 濒死次数。计数型，无事件即 0。
	static float ComputeSurvivalPressure(const FFloorBehaviorSnapshot& Snapshot);

	// --- 常量（测试直接引用，避免魔法数字两处漂移）---
	static constexpr float PrimaryBuildThreshold = 85.f;  // 集中度超过它才算"主力打法"
	static constexpr float ConfidenceInitial     = 0.5f;
	static constexpr float ConfidenceStep        = 0.2f;
	static constexpr float ConfidenceMax         = 0.95f;

	// 数据不足时的中性值——不偏向"玩家强"也不偏向"玩家弱"
	static constexpr float NeutralScore = 50.f;

	// 战斗效率的映射锚点（秒 / 次，按 MVP 房间规模拍的，W3 有真实数据后再调）
	static constexpr float FastClearSeconds = 15.f;   // 快到这个程度算满分
	static constexpr float SlowClearSeconds = 90.f;   // 慢到这个程度算 0 分
	static constexpr float HitsPerRoomFloor = 0.f;    // 一次不挨打算满分
	static constexpr float HitsPerRoomCeil  = 8.f;    // 每房挨打这么多次算 0 分

	// 策略切换的映射锚点
	static constexpr int32 SwitchCountForFullScore = 6;  // 一层切这么多次算满分
	static constexpr int32 MVPWeaponCount          = 2;  // MVP 只有剑/弓两把

	// 生存压力权重
	static constexpr float LowHpEventWeight = 25.f;
	static constexpr float NearDeathWeight  = 50.f;
};
