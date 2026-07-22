#include "SHMAttributeComponent.h"
#include "SHMEventBus.h"
#include "SHMGameplayTags.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

USHMAttributeComponent::USHMAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USHMAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 初始化 HP 为最大值
	CurrentHP = GetMaxHP();
	bIsDead = false;
	bLowHpFired = false;
}

void USHMAttributeComponent::ApplyDamage(float Amount, AActor* Instigator, FGameplayTag SourceTag)
{
	if (bIsDead || Amount <= 0.f)
	{
		return;
	}

	CurrentHP = FMath::Max(0.f, CurrentHP - Amount);

	// 击杀归因：死亡广播发生在下面，届时 Owner 要靠这两个字段知道"被谁用什么打死的"
	LastDamageSourceTag = SourceTag;
	LastDamageInstigator = Instigator;

	OnDamageTaken.Broadcast(Instigator, Amount);

	// --- 玩家侧事件上报（敌人受击不进画像，画像只记录玩家行为）---
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bOwnerIsPlayer = OwnerPawn && OwnerPawn->IsPlayerControlled();

	if (bOwnerIsPlayer)
	{
		if (USHMEventBus* Bus = USHMEventBus::Get(this))
		{
			// ContextTag = 打我的是谁（Enemy.Rush），SourceTag 留空——玩家挨打时没"用"任何东西
			Bus->BroadcastSimple(SHMTags::Event_Combat_HitTaken, Instigator,
				FGameplayTag(), SourceTag, Amount);

			// 低血量事件：跌破阈值只报一次，回血超过阈值后重新武装
			const float Ratio = GetHPRatio();
			if (!bLowHpFired && Ratio > 0.f && Ratio < LowHpThreshold)
			{
				bLowHpFired = true;
				Bus->BroadcastSimple(SHMTags::Event_Combat_LowHp, GetOwner(),
					FGameplayTag(), FGameplayTag(), Ratio);
			}
		}
	}

	// Sprint 1 临时调试显示（HP 条 UI 还没做好前用来肉眼确认扣血）
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, FString::Printf(
			TEXT("%s 受到 %.0f 伤害，剩余 HP %.0f/%.0f"),
			*GetOwner()->GetName(), Amount, CurrentHP, GetMaxHP()));
	}

	if (CurrentHP <= 0.f)
	{
		bIsDead = true;

		if (bOwnerIsPlayer)
		{
			if (USHMEventBus* Bus = USHMEventBus::Get(this))
			{
				Bus->BroadcastSimple(SHMTags::Event_Combat_Death, GetOwner());
			}
		}

		OnDeath.Broadcast();
	}
}

void USHMAttributeComponent::ApplyHeal(float Amount)
{
	if (bIsDead || Amount <= 0.f)
	{
		return;
	}

	CurrentHP = FMath::Min(GetMaxHP(), CurrentHP + Amount);
	OnHealed.Broadcast(Amount, CurrentHP);

	// 回到阈值以上，重新武装低血量事件
	if (GetHPRatio() >= LowHpThreshold)
	{
		bLowHpFired = false;
	}
}
