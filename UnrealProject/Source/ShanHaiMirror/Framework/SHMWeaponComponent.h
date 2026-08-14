#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SHMCoreTypes.h"
#include "SHMWeaponComponent.generated.h"

class USHMAttributeComponent;

// ============================================================================
// 武器组件 —— 管理主/副武器切换 + 提供当前武器数据给攻击判定
// Sprint 1：只暴露接口，攻击判定在 SMCharacter 里临时硬编码（扇形近战）
// Sprint 2：实现切武器 + 远程投射物，届时这里接真正的攻击逻辑
// ============================================================================
UCLASS(ClassGroup = (SHM), meta = (BlueprintSpawnableComponent))
class SHANHAIMIRROR_API USHMWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMWeaponComponent();

	// --- 当前活跃武器数据（读给攻击判定用） ---
	UFUNCTION(BlueprintPure)
	FGameplayTag GetActiveWeaponTag() const { return ActiveWeaponTag; }

	UFUNCTION(BlueprintPure)
	EAttackPattern GetActiveAttackPattern() const { return ActiveAttackPattern; }

	/**
	 * 当前武器伤害。
	 *
	 * ⚠️ 实现在 .cpp —— 因为它会叠加 D-25 的调试倍率。
	 * 这是**全项目唯一的玩家伤害读取点**（SMCharacter 的近战与远程都走它），
	 * 所以倍率只要在这里叠一次，不会散落到各处、也不会漏掉某条攻击路径。
	 */
	UFUNCTION(BlueprintPure)
	float GetActiveBaseDamage() const;

	/** 未经调试倍率修饰的原始伤害。给 UI/测试用，避免把作弊值当成配置值展示。 */
	UFUNCTION(BlueprintPure)
	float GetActiveBaseDamageRaw() const { return ActiveBaseDamage; }

	UFUNCTION(BlueprintPure)
	float GetActiveAttackSpeed() const { return ActiveAttackSpeed; }

	// --- 武器切换（S2-F2 实现） ---
	UFUNCTION(BlueprintCallable)
	void SwitchWeapon();

	UFUNCTION(BlueprintPure)
	bool IsMainWeaponActive() const { return bMainWeaponActive; }

	// --- 给外部用于设置初始武器（从 DataTable 或角色配置） ---
	UFUNCTION(BlueprintCallable)
	void SetMainWeapon(FGameplayTag Tag, EAttackPattern Pattern, float Damage, float Speed, float Range);

	UFUNCTION(BlueprintCallable)
	void SetSubWeapon(FGameplayTag Tag, EAttackPattern Pattern, float Damage, float Speed, float Range);

protected:
	virtual void BeginPlay() override;

private:
	// 当前活跃
	UPROPERTY(VisibleAnywhere, Category = "Weapon|Active")
	FGameplayTag ActiveWeaponTag;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Active")
	EAttackPattern ActiveAttackPattern = EAttackPattern::Melee_Sweep;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Active")
	float ActiveBaseDamage = 20.f;

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Active")
	float ActiveAttackSpeed = 0.4f;

	// 主/副武器槽（重量级数据在 DT_Weapon，这里只存运行时需要的字段）
	bool bMainWeaponActive = true;

	FGameplayTag MainWeaponTag;
	EAttackPattern MainWeaponPattern = EAttackPattern::Melee_Sweep;
	float MainWeaponDamage = 20.f;
	float MainWeaponSpeed = 0.4f;

	FGameplayTag SubWeaponTag;
	EAttackPattern SubWeaponPattern = EAttackPattern::Ranged_Shot;
	float SubWeaponDamage = 12.f;
	float SubWeaponSpeed = 0.6f;
};
