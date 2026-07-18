#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "SMCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USHMAttributeComponent;
class USHMWeaponComponent;
class USHMAbilityComponent;
class USHMBuildComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// ============================================================================
// ASMCharacter —— 所有可玩角色的 C++ 基类
// 职责：持有组件 + 接收输入 + 转发给组件。不写具体武器逻辑，不写数值。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API ASMCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASMCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- 组件（C++ 创建，蓝图可见） ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USHMAttributeComponent> AttributeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USHMWeaponComponent> WeaponComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USHMAbilityComponent> AbilityComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USHMBuildComponent> BuildComp;

	// --- 摄像机 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// --- Enhanced Input 资产指针（蓝图 BP_Lishi 里赋值） ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Aim;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Dodge;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_SwitchWeapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill2;

protected:
	// --- Enhanced Input 回调 ---
	void OnMove(const FInputActionValue& Value);
	void OnAim(const FInputActionValue& Value);
	void OnAttack();
	void OnDodge();
	void OnSwitchWeapon();
	void OnSkill1();
	void OnSkill2();

	// --- 攻击 ---
	void PerformMeleeAttack();

private:
	// 攻击冷却
	float LastAttackTime = -999.f;

	// 鼠标世界位置（Tick 里每帧更新）
	FVector CachedMouseWorldLocation = FVector::ZeroVector;

	void UpdateRotationToMouse();
	void UpdateMouseWorldLocation();

	// 死亡回调绑定
	UFUNCTION()
	void OnOwnerDeath();
};
