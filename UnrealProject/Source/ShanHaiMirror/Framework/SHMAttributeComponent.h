#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
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
	// SourceTag = 伤害来源标签（Weapon.Bow / Enemy.Rush）。
	// 它有两个用途，缺一不可：
	//   ① 画像的 DamageByTag 统计
	//   ② 击杀归因——死亡时要知道"是被哪把武器打死的"，靠 LastDamageSourceTag 留存
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyDamage(float Amount, AActor* Instigator = nullptr, FGameplayTag SourceTag = FGameplayTag());

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ApplyHeal(float Amount);

	// --- 击杀归因（供 Owner 在 OnDeath 时读取）---
	UFUNCTION(BlueprintPure, Category = "Attributes")
	FGameplayTag GetLastDamageSourceTag() const { return LastDamageSourceTag; }

	UFUNCTION(BlueprintPure, Category = "Attributes")
	AActor* GetLastDamageInstigator() const { return LastDamageInstigator.Get(); }

	// --- 低血量阈值（跌破时广播 Event.Combat.LowHp，对齐 GDD §6.4 维度 5）---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float LowHpThreshold = 0.2f;

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
	bool bIsDead = false;      // 防止重复死亡广播
	bool bLowHpFired = false;  // 防止 HP 在阈值附近反复横跳时刷屏广播

	// 击杀归因用：记住最后一次伤害是谁、用什么造成的
	FGameplayTag LastDamageSourceTag;
	TWeakObjectPtr<AActor> LastDamageInstigator;
};
