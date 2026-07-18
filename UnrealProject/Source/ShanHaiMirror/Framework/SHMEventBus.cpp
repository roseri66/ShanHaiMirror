#include "SHMEventBus.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

const FName USHMEventBus::Tag_OnAttack   = FName("Event.Combat.OnAttack");
const FName USHMEventBus::Tag_OnHitTaken = FName("Event.Combat.OnHitTaken");
const FName USHMEventBus::Tag_OnKill     = FName("Event.Combat.OnKill");
const FName USHMEventBus::Tag_OnDodge    = FName("Event.Combat.OnDodge");
const FName USHMEventBus::Tag_OnDeath    = FName("Event.Combat.OnDeath");

void USHMEventBus::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USHMEventBus::Deinitialize()
{
	Super::Deinitialize();
}

void USHMEventBus::Broadcast(FGameplayTag EventTag, AActor* Source)
{
	OnCombatEvent.Broadcast(EventTag, Source);
}

USHMEventBus* USHMEventBus::Get(const UObject* WorldContextObject)
{
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		return GI->GetSubsystem<USHMEventBus>();
	}
	return nullptr;
}
