#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SHMProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

// ============================================================================
// 通用投射物 —— 玩家的弓与 Shooter 敌人共用一个类，差异只在 Init 参数
//
// 伤害携带 SourceTag（Weapon.Bow / Enemy.Shooter），命中时原样传给
// AttributeComponent::ApplyDamage——画像统计与击杀归因靠它，丢了标签
// 这次攻击就在 AI 眼里不存在。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API ASHMProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASHMProjectile();

	// 发射方在 Spawn 后立即调用。Speed 决定射程（配合 LifeSpan ≈ 30m）
	void InitProjectile(float InDamage, FGameplayTag InSourceTag, AActor* InShooter, float InSpeed = 2000.f);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	float Damage = 10.f;
	FGameplayTag SourceTag;
	TWeakObjectPtr<AActor> Shooter;
};
