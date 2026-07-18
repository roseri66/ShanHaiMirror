#include "SHMEnemy.h"
#include "Framework/SHMAttributeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"

ASHMEnemy::ASHMEnemy()
{
	PrimaryActorTick.bCanEverTick = false;

	AttributeComp = CreateDefaultSubobject<USHMAttributeComponent>(TEXT("AttributeComp"));

	// 敌人不需要玩家一样的移动响应——由 AI Controller / BT 驱动
	GetCharacterMovement()->bOrientRotationToMovement = true;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	ArchetypeTag = FGameplayTag::RequestGameplayTag("Enemy.Grunt");
}

void ASHMEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (AttributeComp)
	{
		AttributeComp->OnDeath.AddDynamic(this, &ASHMEnemy::OnOwnerDeath);
	}

	// 撞到玩家造成接触伤害
	GetCapsuleComponent()->SetNotifyRigidBodyCollision(true);
	GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ASHMEnemy::OnCapsuleHit);

	// 运行行为树
	if (BehaviorTreeAsset)
	{
		if (AAIController* AIC = Cast<AAIController>(GetController()))
		{
			AIC->RunBehaviorTree(BehaviorTreeAsset);
		}
	}
}

void ASHMEnemy::InitFromDataTable(FGameplayTag InArchetypeTag, float InHP, float InSpeed,
	float InContactDamage, float InAttackRange, float InAttackCooldown, int32 InThreatValue)
{
	ArchetypeTag = InArchetypeTag;
	ThreatValue = InThreatValue;

	CachedAttackRange = InAttackRange;
	CachedAttackCooldown = InAttackCooldown;
	CachedContactDamage = InContactDamage;

	if (AttributeComp)
	{
		AttributeComp->Health.Base = InHP;
		AttributeComp->CurrentHP = InHP;
	}

	GetCharacterMovement()->MaxWalkSpeed = InSpeed;
}

void ASHMEnemy::OnOwnerDeath()
{
	// 禁用 AI
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		if (AIC->BrainComponent)
		{
			AIC->BrainComponent->StopLogic("Dead");
		}
	}

	// 禁用碰撞
	SetActorEnableCollision(false);

	// 延迟销毁（给死亡动画/特效时间）
	SetLifeSpan(2.f);
}

void ASHMEnemy::OnCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (!OtherPawn || !OtherPawn->IsPlayerControlled())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactDamageTime < CachedAttackCooldown)
	{
		return;
	}
	LastContactDamageTime = Now;

	if (USHMAttributeComponent* OtherAttr = OtherPawn->FindComponentByClass<USHMAttributeComponent>())
	{
		OtherAttr->ApplyDamage(CachedContactDamage, this);
	}
}
