#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SMCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class SHANHAIMIRROR_API ASMCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASMCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** 武器网格体（挂载点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	USceneComponent* WeaponSocket;

protected:
	/** 移动输入 */
	void MoveX(float Value);
	void MoveY(float Value);

	/** 冲刺 */
	void OnDash();

	/** 攻击 */
	void OnAttack();

private:
	/** 移动速度 */
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MoveSpeed = 600.0f;

	/** 冲刺冷却 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float DashCooldown = 2.0f;

	float LastDashTime = 0.0f;
};
