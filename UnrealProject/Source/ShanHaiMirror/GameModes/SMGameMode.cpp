#include "SMGameMode.h"
#include "ShanHaiMirror/Player/SMPlayerController.h"
#include "ShanHaiMirror/Characters/SMCharacter.h"
#include "ShanHaiMirror/UI/SHMDirectorHUD.h"

ASMGameMode::ASMGameMode()
{
	PlayerControllerClass = ASMPlayerController::StaticClass();
	DefaultPawnClass = ASMCharacter::StaticClass();

	// 导演报告卡走 C++ HUD，不需要在编辑器里配任何资产
	HUDClass = ASHMDirectorHUD::StaticClass();
}

void ASMGameMode::BeginPlay()
{
	Super::BeginPlay();
}
