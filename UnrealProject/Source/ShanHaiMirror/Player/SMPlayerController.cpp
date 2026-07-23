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

	// 让游戏视口从第一帧就持有输入焦点。
	// 不设这一步时，bShowMouseCursor=true 会导致视口启动时没有键盘焦点，
	// 键盘（WASD 移动）要等鼠标点一下视口才生效——表现为"必须先点一下才能动"。
	// GameOnly 把全部输入路由给游戏，同时 bShowMouseCursor=true 仍保留光标显示，
	// GetHitResultUnderCursor（瞄准/攻击方向）照常工作。
	//
	// SetConsumeCaptureMouseDown(false) 必须显式关：默认 true 会吞掉"建立鼠标捕获
	// 的那一次按下"——每次捕获丢失（切窗/Alt+Tab）后的第一次左/右键只用于重新捕获、
	// 不进输入系统，表现为攻击/闪避间歇性失灵（踩坑记录 #14）。
	FInputModeGameOnly InputMode;
	InputMode.SetConsumeCaptureMouseDown(false);
	SetInputMode(InputMode);

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
