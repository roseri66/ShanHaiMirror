#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHMAttributeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamageTaken, AActor*, Instigator, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealed, float, Amount, float, NewHP);

// ============================================================================
// 单个属性（Base + FlatBonus + PctBonus）→ Final = (Base + ΣFlat) × (1 + ΣPct)
// 规则修改器和被动强化都通过往 Bonus 数组加项来生效，不需要直接改 Base
// ============================================================================
USTRUCT(BlueprintType)
struct FSHMAttribute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Base = 0.f;

	// 运行时累加的临时修改（来自规则/被动），结构体内部维护，外部通过 AddBonus/RemoveBonus 操作
	float FlatBonus = 0.f;
	float PctBonus = 0.f;

	float GetFinal() const { return (Base + FlatBonus) * (1.f + PctBonus); }
};

// ============================================================================
// 轻量属性组件 —— 负责 HP + 受击/死亡广播
// ============================================================================
UCLASS(ClassGroup = (SHM), meta = (BlueprintSpawnableComponent))
class SHANHAIMIRROR_API USHMAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMAttributeComponent();

	// --- 属性 ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes")
	FSHMAttribute Health;

	// 当前 HP（受击/回血后更新）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	float CurrentHP = 100.f;

	// --- 查询 ---
	UFUNCTION(BlueprintPure)
	float GetMaxHP() const { return Health.GetFinal(); }

	UFUNCTION(BlueprintPure)
	float GetHPRatio() const { return GetMaxHP() > 0.f ? CurrentHP / GetMaxHP() : 0.f; }

	UFUNCTION(BlueprintPure)
	bool IsDead() const { return CurrentHP <= 0.f; }

	// --- 修改 ---
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyDamage(float Amount, AActor* Instigator = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyHeal(float Amount);

	// --- 事件 ---
	UPROPERTY(BlueprintAssignable)
	FOnDamageTaken OnDamageTaken;

	UPROPERTY(BlueprintAssignable)
	FOnDeath OnDeath;

	UPROPERTY(BlueprintAssignable)
	FOnHealed OnHealed;

protected:
	virtual void BeginPlay() override;

private:
	bool bIsDead = false;  // 防止重复死亡广播
};
