#include "SHMBuildComponent.h"

USHMBuildComponent::USHMBuildComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USHMBuildComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USHMBuildComponent::AddPassive(FGameplayTag PassiveTag, FGameplayTagContainer BuildTags, float Value)
{
	// S4-F1：这里往属性组件加 Bonus，更新 AccumulatedBuildTags
}

void USHMBuildComponent::RemovePassive(FGameplayTag PassiveTag)
{
	// S4-F1
}
