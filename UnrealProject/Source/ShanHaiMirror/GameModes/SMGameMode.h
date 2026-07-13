#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SMGameMode.generated.h"

UCLASS()
class SHANHAIMIRROR_API ASMGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASMGameMode();

	virtual void BeginPlay() override;
};
