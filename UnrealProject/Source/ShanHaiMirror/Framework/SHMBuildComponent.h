#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SHMBuildComponent.generated.h"

// ============================================================================
// Build 组件 —— 被动强化叠加（S4-F1 实现）
// Sprint 1-3：组件存在但为空，不影响战斗
// Sprint 4：AddPassive / RemovePassive / 应用到属性组件 Bonus
// ============================================================================
UCLASS(ClassGroup = (SHM), meta = (BlueprintSpawnableComponent))
class SHANHAIMIRROR_API USHMBuildComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMBuildComponent();

	// --- S4-F1 实现 ---
	UFUNCTION(BlueprintCallable)
	void AddPassive(FGameplayTag PassiveTag, FGameplayTagContainer BuildTags, float Value);

	UFUNCTION(BlueprintCallable)
	void RemovePassive(FGameplayTag PassiveTag);

	UFUNCTION(BlueprintPure)
	const TArray<FGameplayTag>& GetActivePassives() const { return ActivePassives; }

	UFUNCTION(BlueprintPure)
	const FGameplayTagContainer& GetAccumulatedBuildTags() const { return AccumulatedBuildTags; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Build")
	TArray<FGameplayTag> ActivePassives;

	UPROPERTY(VisibleAnywhere, Category = "Build")
	FGameplayTagContainer AccumulatedBuildTags;
};
