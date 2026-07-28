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
	class ASHMDirectorHUD* GetDirectorHUD() const;
	bool IsPlayerDead() const;

	// 层间过场时长。LLM 往返被它掩盖——但若决策来得更慢，StartFloor 会继续等
	// （见 MaxDecisionWaitSeconds），绝不用垫底决策抢跑，否则这一层的 LLM 白调用了
	static constexpr float FloorTransitionSeconds = 2.5f;
	// 必须大于 FSHMLlmProvider 的超时（默认 10s），否则玩法层会先于 LLM 放弃，
	// 等待就成了空耗——LLM 那次调用照发不误，结果却没人要
	static constexpr float MaxDecisionWaitSeconds = 12.f;
	static constexpr float DecisionPollSeconds    = 0.2f;

	int32 FloorIndex = 0;
	int32 RoomIndex  = 0;
	float RoomStartTime = 0.f;

	// 异步决策是否还在路上
	bool  bDecisionPending   = false;
	float DecisionWaitedTime = 0.f;

	FDirectorDecision CurrentDecision;

	// 最近一次算出的画像——报告卡要显示「我看到了什么」，
	// 而决策本身不携带画像（FDirectorDecision 是给玩法层的，不含诊断数据）
	FPlayerProfile LastProfile;

	FTimerHandle DelayTimer;
};
