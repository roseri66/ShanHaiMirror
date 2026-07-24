#include "SHMProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Framework/SHMAttributeComponent.h"
#include "Enemies/SHMEnemy.h"
#include "UObject/ConstructorHelpers.h"

ASHMProjectile::ASHMProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(12.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);         // 命中角色/敌人
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);  // 撞墙销毁
	RootComponent = Sphere;

	// 占位外观：引擎自带球体（已退赛，D-07 的资产合规限制随之失效，占位可用引擎内容）
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.15f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->ProjectileGravityScale = 0.f;   // 俯视角平飞
	Movement->bRotationFollowsVelocity = true;

	// 速度 2000 × 1.5s ≈ 30m 后自毁（未命中时）
	InitialLifeSpan = 1.5f;
}

void ASHMProjectile::BeginPlay()
{
	Super::BeginPlay();
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ASHMProjectile::OnSphereOverlap);
}

void ASHMProjectile::InitProjectile(float InDamage, FGameplayTag InSourceTag, AActor* InShooter, float InSpeed)
{
	Damage    = InDamage;
	SourceTag = InSourceTag;
	Shooter   = InShooter;

	Movement->InitialSpeed = InSpeed;
	Movement->MaxSpeed     = InSpeed;
	Movement->Velocity     = GetActorForwardVector() * InSpeed;

	if (InShooter)
	{
		Sphere->IgnoreActorWhenMoving(InShooter, true);
	}
}

void ASHMProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == Shooter.Get())
	{
		return;
	}

	// 不打友军：敌人射的箭穿过其他敌人（MVP 无友伤，Shooter 站后排不许误伤前排）
	if (Shooter.IsValid() && Shooter->IsA<ASHMEnemy>() && OtherActor->IsA<ASHMEnemy>())
	{
		return;
	}

	// 其他投射物之间互不作用
	if (OtherActor->IsA<ASHMProjectile>())
	{
		return;
	}

	if (USHMAttributeComponent* VictimAttr = OtherActor->FindComponentByClass<USHMAttributeComponent>())
	{
		VictimAttr->ApplyDamage(Damage, Shooter.Get(), SourceTag);
	}

	// 命中任何东西（角色或墙）都销毁——穿透是后续武器词条的事，MVP 不做
	Destroy();
}
