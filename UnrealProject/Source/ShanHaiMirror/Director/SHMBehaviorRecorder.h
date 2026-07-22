#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Framework/SHMCoreTypes.h"
#include "Framework/SHMGameplayEvent.h"
#include "SHMBehaviorRecorder.generated.h"

// ============================================================================
// 行为记录器 —— AI Director 链路第 ① 步 OBSERVE
//
// 职责：订阅事件总线，把玩法事件累积成 FFloorBehaviorSnapshot。
//
// **明确不做**（TDD §1.2 模块边界，本类最容易被破坏的一条）：
//   ✗ 不打分、不归一化、不做任何"玩家打得好不好"的判断
//   ✗ 不读 GetWorld()->GetTimeSeconds()——耗时由事件的 Magnitude 传入
//   ✗ 不 Cast Actor、不 FindComponentByClass——数据必须已在事件里
//
// 上面三条合起来保证了一件事：**RecordEvent 可以在没有引擎世界的情况下被调用**，
// 单测直接 NewObject 然后灌事件即可，不需要起 World、不需要 PIE。
//
// 为什么是 GameInstanceSubsystem 而不是 WorldSubsystem：
//   一层 = 多个房间 = 多次 Level Streaming/切图，WorldSubsystem 会随 World 销毁，
//   层内累积的数据就断了；跨层历史更是必须活到整个 Run 结束。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API USHMBehaviorRecorder : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------------------------------------------------------------------
	// 纯数据入口 —— 单测的注入点，不碰世界
	// ---------------------------------------------------------------------
	void RecordEvent(const FSHMGameplayEvent& Event);

	// ---------------------------------------------------------------------
	// 层生命周期
	// ---------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "AI Director")
	void BeginFloor(int32 FloorIndex);

	// 定稿当前层：压入历史并返回它的引用
	const FFloorBehaviorSnapshot& FinalizeFloor();

	// 整个 Run 重开（死亡/重新开始）
	UFUNCTION(BlueprintCallable, Category = "AI Director")
	void ResetRun();

	// ---------------------------------------------------------------------
	// 查询
	// ---------------------------------------------------------------------
	const FFloorBehaviorSnapshot& GetCurrentSnapshot() const { return Current; }
	const TArray<FFloorBehaviorSnapshot>& GetHistory() const { return History; }

	UFUNCTION(BlueprintPure, Category = "AI Director")
	int32 GetCurrentFloorIndex() const { return Current.FloorIndex; }

	// 调试用：把当前快照打成一行字符串（W2 之前肉眼验证数据用）
	FString DebugSnapshotToString() const;

private:
	// 事件总线回调 —— 只做转发，逻辑全在 RecordEvent 里，保证两条路径行为一致
	UFUNCTION()
	void HandleGameplayEvent(const FSHMGameplayEvent& Event);

	FFloorBehaviorSnapshot Current;
	TArray<FFloorBehaviorSnapshot> History;
};
