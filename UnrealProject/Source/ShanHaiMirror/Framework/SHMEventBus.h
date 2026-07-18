#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SHMEventBus.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatEvent, FGameplayTag, EventTag, AActor*, Source);

// ============================================================================
// 全局事件总线 —— GameInstanceSubsystem
// 玩法系统广播事件，BehaviorRecorder / UI / 音效各自订阅
// Sprint 1：创建骨架 + 攻击/死亡事件广播
// Sprint 3：BehaviorRecorder 订阅全部事件
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API USHMEventBus : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 通用事件委托 ---
	// 玩法系统通过这个广播，订阅方按 EventTag 过滤自己关心的事件
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCombatEvent OnCombatEvent;

	// --- 便捷广播（C++ 内调用） ---
	UFUNCTION(BlueprintCallable, Category = "Events")
	void Broadcast(FGameplayTag EventTag, AActor* Source);

	// --- 获取实例 ---
	UFUNCTION(BlueprintPure, Category = "Events", meta = (WorldContext = "WorldContextObject"))
	static USHMEventBus* Get(const UObject* WorldContextObject);

private:
	// 预定义事件标签（Sprint 1 会用到的）
	static const FName Tag_OnAttack;
	static const FName Tag_OnHitTaken;
	static const FName Tag_OnKill;
	static const FName Tag_OnDodge;
	static const FName Tag_OnDeath;
};
