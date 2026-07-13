#include "SMCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"

ASMCharacter::ASMCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 俯视角：锁定角色朝向为鼠标方向，不随移动转向
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	// 武器挂载点
	WeaponSocket = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSocket"));
	WeaponSocket->SetupAttachment(GetMesh(), TEXT("WeaponSocket"));
}

void ASMCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ASMCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ASMCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveX", this, &ASMCharacter::MoveX);
	PlayerInputComponent->BindAxis("MoveY", this, &ASMCharacter::MoveY);
	PlayerInputComponent->BindAction("Dash", IE_Pressed, this, &ASMCharacter::OnDash);
	PlayerInputComponent->BindAction("Attack", IE_Pressed, this, &ASMCharacter::OnAttack);
}

void ASMCharacter::MoveX(float Value)
{
	if (Value != 0.0f)
	{
		AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
	}
}

void ASMCharacter::MoveY(float Value)
{
	if (Value != 0.0f)
	{
		AddMovementInput(FVector(0.0f, 1.0f, 0.0f), Value);
	}
}

void ASMCharacter::OnDash()
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastDashTime < DashCooldown)
	{
		return;
	}
	LastDashTime = CurrentTime;

	FVector DashDirection = GetLastMovementInputVector();
	if (DashDirection.IsNearlyZero())
	{
		DashDirection = GetActorForwardVector();
	}

	FVector DashVelocity = DashDirection * 2000.0f;
	LaunchCharacter(DashVelocity, false, false);
}

void ASMCharacter::OnAttack()
{
	// 攻击逻辑（后续接入武器系统）
}
