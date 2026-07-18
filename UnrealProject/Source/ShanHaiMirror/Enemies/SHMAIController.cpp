#include "SHMAIController.h"
#include "Kismet/GameplayStatics.h"

ASHMAIController::ASHMAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASHMAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		MoveToActor(PlayerPawn, AcceptanceRadius);
	}
}
