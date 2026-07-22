#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SHMGameplayEvent.h"
#include "SHMEventBus.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayEvent, const FSHMGameplayEvent&, Event);

// ============================================================================
// 全局事件总线 —— GameInstanceSubsystem
//
// 职责：广播玩法事件。**不存状态、不做业务逻辑**（TDD §1.2 模块边界）。
// 订阅方：BehaviorRecorder（画像数据源）、UI、音效。
//
// 为什么是 GameInstanceSubsystem 而不是 WorldSubsystem：
//   事件要跨关卡切换存活（一层 = 多个房间 = 多次 Level Streaming），
//   WorldSubsystem 会随 World 一起销毁，层内数据就断了。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API USHMEventBus : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 订阅方按 Event.EventTag 自行过滤
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnGameplayEvent OnGameplayEvent;

	// --- 广播 ---
	UFUNCTION(BlueprintCallable, Category = "Events")
	void Broadcast(const FSHMGameplayEvent& Event);

	// 便捷重载（C++ 调用点用，省去构造临时对象）
	void BroadcastSimple(FGameplayTag EventTag, AActor* Instigator = nullptr,
	                     FGameplayTag SourceTag = FGameplayTag(),
	                     FGameplayTag ContextTag = FGameplayTag(),
	                     float Magnitude = 0.f, bool bSuccess = false);

	// --- 获取实例 ---
	UFUNCTION(BlueprintPure, Category = "Events", meta = (WorldContext = "WorldContextObject"))
	static USHMEventBus* Get(const UObject* WorldContextObject);
};
