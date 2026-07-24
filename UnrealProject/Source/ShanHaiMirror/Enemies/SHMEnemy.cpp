#include "SHMEnemy.h"
#include "SHMEnemyTable.h"
#include "SHMAIController.h"
#include "Combat/SHMProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Framework/SHMAttributeComponent.h"
#include "Framework/SHMEventBus.h"
#include "Framework/SHMGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BrainComponent.h"

ASHMEnemy::ASHMEnemy()
{
	PrimaryActorTick.bCanEverTick = false;

	AttributeComp = CreateDefaultSubobject<USHMAttributeComponent>(TEXT("AttributeComp"));

	// 敌人不需要玩家一样的移动响应——由 AI Controller / BT 驱动
	GetCharacterMovement()->bOrientRotationToMovement = true;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 代码生成的敌人默认用 C++ AIController（追击/保距逻辑在里面）；
	// 手工蓝图（BP_Grunt）如有自己的 AIC 覆盖值不受影响
	AIControllerClass = ASHMAIController::StaticClass();

	// 用 native tag，不用 RequestGameplayTag——后者在 CDO 构造阶段若标签表尚未加载会 ensure
	ArchetypeTag = SHMTags::Enemy_Grunt;
}

void ASHMEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (AttributeComp)
	{
		AttributeComp->OnDeath.AddDynamic(this, &ASHMEnemy::OnOwnerDeath);
		AttributeComp->OnDamageTaken.AddDynamic(this, &ASHMEnemy::OnDamaged);
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

void ASHMEnemy::InitFromRow(const FSHMEnemyRow& Row)
{
	InitFromDataTable(Row.ArchetypeTag, Row.HP, Row.Speed, Row.ContactDamage,
		Row.AttackRange, Row.AttackCooldown, Row.ThreatValue);

	// 原型识别色：运行时挂几何外观 + 染色。
	// 刻意不放构造函数——BP_Grunt 这类手工蓝图有自己的外观，只有代码生成的
	// 变体（走 InitFromRow）才需要占位视觉，避免蓝图实例出现双重外观。
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMesh)
	{
		UStaticMeshComponent* Visual = NewObject<UStaticMeshComponent>(this, TEXT("ArchetypeVisual"));
		Visual->SetStaticMesh(CubeMesh);
		Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Visual->SetupAttachment(GetCapsuleComponent());
		Visual->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.7f));
		Visual->RegisterComponent();

		if (BaseMat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(TEXT("Color"), Row.Color);
			Visual->SetMaterial(0, MID);
		}
	}
}

void ASHMEnemy::TryRangedAttack(AActor* Target)
{
	if (!Target) { return; }

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRangedAttackTime < CachedAttackCooldown)
	{
		return;
	}
	LastRangedAttackTime = Now;

	FVector Dir = Target->GetActorLocation() - GetActorLocation();
	Dir.Z = 0.f;
	Dir = Dir.IsNearlyZero() ? GetActorForwardVector() : Dir.GetSafeNormal();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ASHMProjectile* Proj = GetWorld()->SpawnActor<ASHMProjectile>(
		ASHMProjectile::StaticClass(), GetActorLocation() + Dir * 80.f, Dir.Rotation(), Params))
	{
		// 敌方投射物慢于玩家的（可闪避），SourceTag 用原型标签供玩家 HitTaken 归因
		Proj->InitProjectile(CachedContactDamage, ArchetypeTag, this, 1200.f);
	}
}

void ASHMEnemy::OnDamaged(AActor* DamageInstigator, float Damage)
{
	// 只统计玩家造成的伤害——敌人互相误伤、环境伤害不进画像
	const APawn* InstigatorPawn = Cast<APawn>(DamageInstigator);
	if (!InstigatorPawn || !InstigatorPawn->IsPlayerControlled())
	{
		return;
	}

	if (USHMEventBus* Bus = USHMEventBus::Get(this))
	{
		// SourceTag = 玩家用的武器（AttributeComp 刚存下的），ContextTag = 我是什么怪
		Bus->BroadcastSimple(SHMTags::Event_Combat_DamageDealt, DamageInstigator,
			AttributeComp ? AttributeComp->GetLastDamageSourceTag() : FGameplayTag(),
			ArchetypeTag, Damage);
	}
}

void ASHMEnemy::OnOwnerDeath()
{
	// 击杀上报 —— 必须在禁用碰撞/销毁之前发，否则数据就丢了
	if (AttributeComp)
	{
		const APawn* KillerPawn = Cast<APawn>(AttributeComp->GetLastDamageInstigator());
		if (KillerPawn && KillerPawn->IsPlayerControlled())
		{
			if (USHMEventBus* Bus = USHMEventBus::Get(this))
			{
				Bus->BroadcastSimple(SHMTags::Event_Combat_Kill,
					AttributeComp->GetLastDamageInstigator(),
					AttributeComp->GetLastDamageSourceTag(),  // 被哪把武器打死的
					ArchetypeTag);                            // 死的是什么怪
			}
		}
	}

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
		// 传自己的原型标签——玩家的 HitTaken 事件靠它知道"是被什么怪打的"
		OtherAttr->ApplyDamage(CachedContactDamage, this, ArchetypeTag);
	}
}
