#include "SHMEventBus.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void USHMEventBus::Broadcast(const FSHMGameplayEvent& Event)
{
	OnGameplayEvent.Broadcast(Event);
}

void USHMEventBus::BroadcastSimple(FGameplayTag EventTag, AActor* Instigator,
	FGameplayTag SourceTag, FGameplayTag ContextTag, float Magnitude, bool bSuccess)
{
	FSHMGameplayEvent Event(EventTag, SourceTag, ContextTag, Magnitude, bSuccess);
	Event.Instigator = Instigator;
	Broadcast(Event);
}

USHMEventBus* USHMEventBus::Get(const UObject* WorldContextObject)
{
	if (const UGameInstance* GI = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		return GI->GetSubsystem<USHMEventBus>();
	}
	return nullptr;
}
