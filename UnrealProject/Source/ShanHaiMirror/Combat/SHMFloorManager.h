#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Framework/SHMCoreTypes.h"
#include "SHMFloorManager.generated.h"

// ============================================================================
// 层管理器 —— 闭环的合龙点
//
//   房间×N 清空 → FinalizeFloor → Analyzer(真实画像) → DirectorCore
//     → 下一层应用（刷怪权重给 EncounterManager，规则数值走 DirectorCore 查询）
//     → 白泽台词屏显
//
// 职责边界：只做节奏编排与各子系统串联。不刷怪（EncounterManager）、
// 不算画像（Analyzer）、不做决策（DirectorCore）。
//
// 层结构 = 线性 N 房间同场重刷（DECISIONS D-10：无节点图、无 Level Streaming）。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API USHMFloorManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	static constexpr int32 TotalFloors   = 3;
	static constexpr int32 RoomsPerFloor = 3;

	// 每房威胁预算（与规则的 ChallengeBudget 是两回事：那个管规则花费，这个管刷怪量）
	static int32 RoomThreatBudget(int32 FloorIndex) { return 20 + FloorIndex * 8; }

private:
	void StartRun();
	void StartFloor();
	void StartRoom();
	void HandleRoomCleared();
	void EndFloor();
	void ShowDirectorMessage(const FDirectorDecision& Decision) const;
	bool IsPlayerDead() const;

	int32 FloorIndex = 0;
	int32 RoomIndex  = 0;
	float RoomStartTime = 0.f;

	FDirectorDecision CurrentDecision;
	FTimerHandle DelayTimer;
};
