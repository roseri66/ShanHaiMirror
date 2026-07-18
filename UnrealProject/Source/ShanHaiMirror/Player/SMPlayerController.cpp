#include "SMPlayerController.h"

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

	// Enhanced Input 子系统由 PC 自动加载 Mapping Context
	// 需要在 BP 子类里调用 UEnhancedInputLocalPlayerSubsystem::AddMappingContext
}

bool ASMPlayerController::GetMouseWorldLocation(FVector& OutLocation) const
{
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		OutLocation = Hit.Location;
		return true;
	}
	return false;
}
