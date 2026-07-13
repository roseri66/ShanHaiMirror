#include "SMGameMode.h"
#include "ShanHaiMirror/Player/SMPlayerController.h"
#include "ShanHaiMirror/Characters/SMCharacter.h"

ASMGameMode::ASMGameMode()
{
	PlayerControllerClass = ASMPlayerController::StaticClass();
	DefaultPawnClass = ASMCharacter::StaticClass();
}

void ASMGameMode::BeginPlay()
{
	Super::BeginPlay();
}
