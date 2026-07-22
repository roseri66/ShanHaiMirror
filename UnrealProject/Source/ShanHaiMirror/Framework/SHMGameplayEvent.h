#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SHMGameplayEvent.generated.h"

// ============================================================================
// 玩法事件负载
//
// 设计要点：**事件自带全部数据，订阅方不回头去问 Actor**。
// BehaviorRecorder 只读这个结构体，不 Cast、不 FindComponentByClass——
// 这是它能脱离引擎世界被单测的前提。
//
// 两个标签的分工（容易混，写清楚）：
//   SourceTag  = 玩家用了什么     Weapon.Bow / Ability.Dodge
//   ContextTag = 事件涉及的对象   Enemy.Tank（击杀了谁 / 被谁打）
// 例：用弓射死盾兵 → SourceTag=Weapon.Bow, ContextTag=Enemy.Tank
// ============================================================================
USTRUCT(BlueprintType)
struct SHANHAIMIRROR_API FSHMGameplayEvent
{
	GENERATED_BODY()

	FSHMGameplayEvent() = default;

	FSHMGameplayEvent(FGameplayTag InEventTag, FGameplayTag InSourceTag = FGameplayTag(),
	                  FGameplayTag InContextTag = FGameplayTag(), float InMagnitude = 0.f,
	                  bool bInSuccess = false)
		: EventTag(InEventTag)
		, SourceTag(InSourceTag)
		, ContextTag(InContextTag)
		, Magnitude(InMagnitude)
		, bSuccess(bInSuccess)
	{}

	// 事件类型：Event.Combat.Attack / Event.Flow.FloorFinished ...
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag EventTag;

	// 玩家用的东西：Weapon.Bow / Ability.Dodge
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag SourceTag;

	// 事件涉及的对象：Enemy.Tank
	UPROPERTY(BlueprintReadWrite)
	FGameplayTag ContextTag;

	// 通用数值。语义随 EventTag 变化：
	//   DamageDealt/HitTaken → 伤害量
	//   RoomFinished         → 房间耗时（秒）——注意：由广播方传入，
	//                          Recorder 绝不自己读 GetTimeSeconds()，否则就不可测了
	UPROPERTY(BlueprintReadWrite)
	float Magnitude = 0.f;

	// 布尔结果。语义随 EventTag 变化：Dodge → 是否成功躲开
	UPROPERTY(BlueprintReadWrite)
	bool bSuccess = false;

	// 事件发起者。**仅用于过滤"这是不是玩家干的"，不用于取数据。**
	// 单测时为 nullptr，因此所有必要数据必须已经在上面的字段里。
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> Instigator = nullptr;
};
