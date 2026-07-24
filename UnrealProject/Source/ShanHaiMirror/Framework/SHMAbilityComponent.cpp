#include "SHMAbilityComponent.h"
#include "SHMGameplayTags.h"
#include "Director/SHMDirectorCore.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

USHMAbilityComponent::USHMAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// 预留 2 个技能槽
	SkillCooldowns.Init(0.f, 2);
	LastSkillCastTimes.Init(-999.f, 2);
}

void USHMAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
}

// ========== 闪避 ==========
// S2-F1 完整实现——Sprint 1 只返回 true，让调用的地方不崩即可

bool USHMAbilityComponent::TryDodge(FVector Direction)
{
	// 导演规则作用面：冷却倍率（Rule.Cooldown ×1.15 = 闪避变慢 15%）
	float EffectiveCooldown = DodgeCooldown;
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const USHMDirectorCore* Director = GI->GetSubsystem<USHMDirectorCore>())
			{
				EffectiveCooldown *= Director->GetActiveRuleMultiplier(SHMTags::Rule_Cooldown.GetTag());
			}
		}
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastDodgeTime < EffectiveCooldown)
	{
		return false;
	}
	// S2-F1：这里加位移 + 无敌帧逻辑
	LastDodgeTime = Now;
	OnAbilityUsed.Broadcast(SHMTags::Ability_Dodge);
	return true;
}

bool USHMAbilityComponent::IsDodgeOnCooldown() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return (Now - LastDodgeTime) < DodgeCooldown;
}

float USHMAbilityComponent::GetDodgeCooldownRemaining() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return FMath::Max(0.f, DodgeCooldown - (Now - LastDodgeTime));
}

// ========== 技能 ==========
// S4 实现

bool USHMAbilityComponent::TryCastSkill(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= 2)
	{
		return false;
	}
	// S4：这里加技能逻辑
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	LastSkillCastTimes[SlotIndex] = Now;
	return true;
}

bool USHMAbilityComponent::IsSkillOnCooldown(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= 2)
	{
		return true;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return (Now - LastSkillCastTimes[SlotIndex]) < SkillCooldowns[SlotIndex];
}
