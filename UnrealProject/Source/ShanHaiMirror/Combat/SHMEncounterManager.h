#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "Enemies/SHMEnemyTable.h"   // TMap 按值存行，需要完整类型
#include "SHMEncounterManager.generated.h"

class UDataTable;
class ASHMEnemy;

DECLARE_MULTICAST_DELEGATE(FSHMOnRoomCleared);

// ============================================================================
// 遭遇管理器 —— 消费 FDirectorDecision.EnemyWeights 的执行层
//
// 职责：按导演权重 + 威胁预算刷一房怪、跟踪存活、清空后广播。
// **不做决策**：权重和预算都是外部喂进来的（FloorManager ← DirectorCore）。
//
// 敌人数据源 Data/EnemyTable.csv；生成位置为玩家周围环形随机点——
// 零编辑器摆点，任何有 NavMesh 的关卡都能直接跑。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API USHMEncounterManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 刷一房怪。返回实际生成数（0 = 数据缺失或预算过低，调用方须处理）
	int32 SpawnRoomWave(const TMap<FGameplayTag, float>& Weights, int32 ThreatBudget, const FVector& Center);

	// 房间清空（全部死亡）时广播
	FSHMOnRoomCleared OnRoomCleared;

	int32 GetAliveCount() const;

private:
	void PollRoomState();
	ASHMEnemy* SpawnEnemyOfArchetype(const FGameplayTag& ArchetypeTag, const FVector& Location);

	// 在 Center 周围环形取一个**落在导航网格上**的刷怪点。
	// 多次尝试并逐步收缩半径；全部失败返回 false（调用方跳过该怪并留日志）。
	// 不验证落点的教训：平台外刷怪 → 怪坠落永不死 → 房间永不结算，整局卡死。
	bool FindNavigableSpawnPoint(const FVector& Center, FVector& OutLocation);

	// 本房参考高度：低于它一定幅度的怪视为坠落，清理掉——被击退坠台的怪
	// 和刷歪的怪一样会卡死结算，轮询时兜底
	float RoomCenterZ = 0.f;

	UPROPERTY()
	TObjectPtr<UDataTable> EnemyTable;

	// 原型 → 行（表加载时建索引；AI 只认原型，具体怪从原型池抽——当前每原型一行）
	TMap<FGameplayTag, FSHMEnemyRow> RowsByArchetype;
	TMap<FGameplayTag, int32> ThreatByArchetype;

	UPROPERTY()
	TArray<TObjectPtr<ASHMEnemy>> AliveEnemies;

	FTimerHandle PollTimer;
	FRandomStream Rand;
};
