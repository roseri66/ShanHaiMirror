#include "SHMAttributeComponent.h"
#include "GameFramework/Actor.h"
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
}

void USHMAttributeComponent::ApplyDamage(float Amount, AActor* Instigator)
{
	if (bIsDead || Amount <= 0.f)
	{
		return;
	}

	CurrentHP = FMath::Max(0.f, CurrentHP - Amount);
	OnDamageTaken.Broadcast(Instigator, Amount);

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
		OnDeath.Broadcast();
	}
}

void USHMAttributeComponent::ApplyHeal(float Amount)
{
	if (bIsDead || Amount <= 0.f)
	{
		return;
	}

	const float PrevHP = CurrentHP;
	CurrentHP = FMath::Min(GetMaxHP(), CurrentHP + Amount);
	OnHealed.Broadcast(Amount, CurrentHP);
}
