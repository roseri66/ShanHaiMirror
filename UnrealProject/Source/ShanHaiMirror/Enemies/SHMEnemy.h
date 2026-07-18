#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "Framework/SHMCoreTypes.h"
#include "SHMEnemy.generated.h"

class USHMAttributeComponent;
class UBehaviorTree;
class UDataTable;

// ============================================================================
// ASHMEnemy —— 所有敌人的 C++ 基类
// 职责：持有一个 USHMAttributeComponent + 原型标签 + 威胁值。
// 行为树逻辑由 BP 子类 + 共享 BT Asset 驱动（不在这里写）。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API ASHMEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	ASHMEnemy();

	virtual void BeginPlay() override;

	// --- 属性组件 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USHMAttributeComponent> AttributeComp;

	// --- 原型标签（AI Director 用它调权重） ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FGameplayTag ArchetypeTag;

	// --- 威胁值（遭遇预算用） ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	int32 ThreatValue = 5;

	// --- 行为树（BP 子类里赋值） ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	// --- 初始化——从 DataTable 行填充数据 ---
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitFromDataTable(FGameplayTag InArchetypeTag, float InHP, float InSpeed, float InContactDamage,
		float InAttackRange, float InAttackCooldown, int32 InThreatValue);

protected:
	// 内部缓存的 DataTable 数据
	float CachedAttackRange = 150.f;
	float CachedAttackCooldown = 1.2f;
	float CachedContactDamage = 15.f;

	// 死亡回调
	UFUNCTION()
	void OnOwnerDeath();

	// 撞到玩家造成接触伤害
	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

private:
	float LastContactDamageTime = -999.f;
};
