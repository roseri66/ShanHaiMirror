#include "SMPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"

ASMPlayerController::ASMPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void ASMPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetupTopDownCamera();
}

void ASMPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void ASMPlayerController::SetupTopDownCamera()
{
	if (APawn* ControlledPawn = GetPawn())
	{
		// 俯视角：摄像机放在角色正上方
		USpringArmComponent* SpringArm = NewObject<USpringArmComponent>(ControlledPawn);
		SpringArm->TargetArmLength = CameraHeight;
		SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.0f, 0.0f));
		SpringArm->bDoCollisionTest = false;
		SpringArm->bInheritPitch = false;
		SpringArm->bInheritYaw = false;
		SpringArm->bInheritRoll = false;
		SpringArm->SetupAttachment(ControlledPawn->GetRootComponent());
		SpringArm->RegisterComponent();

		UCameraComponent* TopDownCamera = NewObject<UCameraComponent>(ControlledPawn);
		TopDownCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
		TopDownCamera->RegisterComponent();
	}
}

void ASMPlayerController::OnMoveToPressed()
{
	// 鼠标点击移动（后续实现）
}

void ASMPlayerController::OnMoveX(float Value)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
	}
}

void ASMPlayerController::OnMoveY(float Value)
{
	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(FVector(0.0f, 1.0f, 0.0f), Value);
	}
}
