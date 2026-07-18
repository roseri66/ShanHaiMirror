#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SHMAbilityComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityUsed, FGameplayTag, AbilityTag);

// ============================================================================
// 能力组件 —— 闪避（S2-F1）+ 主动技能 2 槽（S4+）
// Sprint 1：闪避接口预留（逻辑 S2 实现），技能槽留空
// ============================================================================
UCLASS(ClassGroup = (SHM), meta = (BlueprintSpawnableComponent))
class SHANHAIMIRROR_API USHMAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMAbilityComponent();

	// --- 闪避（S2-F1 实现完整逻辑） ---
	UFUNCTION(BlueprintCallable)
	bool TryDodge(FVector Direction);

	UFUNCTION(BlueprintPure)
	bool IsDodgeOnCooldown() const;

	UFUNCTION(BlueprintPure)
	float GetDodgeCooldownRemaining() const;

	// --- 主动技能（S4 实现） ---
	UFUNCTION(BlueprintCallable)
	bool TryCastSkill(int32 SlotIndex);

	UFUNCTION(BlueprintPure)
	bool IsSkillOnCooldown(int32 SlotIndex) const;

	// --- 事件 ---
	UPROPERTY(BlueprintAssignable)
	FOnAbilityUsed OnAbilityUsed;

	// --- 配置（Sprint 1 只暴露配置，S2 读到 AbilityComponent 里用） ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
	float DodgeDistance = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
	float DodgeDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
	float DodgeIFrameDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge")
	float DodgeCooldown = 0.8f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Dodge")
	float LastDodgeTime = -999.f;

	// 技能冷却（S4 用到）
	TArray<float> SkillCooldowns;      // 配置的冷却值
	TArray<float> LastSkillCastTimes;  // 上次释放时间
};
