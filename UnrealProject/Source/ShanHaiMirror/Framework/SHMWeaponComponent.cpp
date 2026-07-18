#include "SHMWeaponComponent.h"

USHMWeaponComponent::USHMWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USHMWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// 默认主武器
	SetMainWeapon(
		FGameplayTag::RequestGameplayTag("Weapon.Sword"),
		EAttackPattern::Melee_Sweep,
		20.f, 0.4f, 200.f
	);
}

void USHMWeaponComponent::SetMainWeapon(FGameplayTag Tag, EAttackPattern Pattern, float Damage, float Speed, float Range)
{
	// Range 预留——S2 弓会用到
	MainWeaponTag = Tag;
	MainWeaponPattern = Pattern;
	MainWeaponDamage = Damage;
	MainWeaponSpeed = Speed;

	if (bMainWeaponActive)
	{
		ActiveWeaponTag = Tag;
		ActiveAttackPattern = Pattern;
		ActiveBaseDamage = Damage;
		ActiveAttackSpeed = Speed;
	}
}

void USHMWeaponComponent::SetSubWeapon(FGameplayTag Tag, EAttackPattern Pattern, float Damage, float Speed, float Range)
{
	SubWeaponTag = Tag;
	SubWeaponPattern = Pattern;
	SubWeaponDamage = Damage;
	SubWeaponSpeed = Speed;

	if (!bMainWeaponActive)
	{
		ActiveWeaponTag = Tag;
		ActiveAttackPattern = Pattern;
		ActiveBaseDamage = Damage;
		ActiveAttackSpeed = Speed;
	}
}

void USHMWeaponComponent::SwitchWeapon()
{
	// S2-F2 实现——Sprint 1 暂不处理
}
