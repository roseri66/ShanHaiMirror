#include "SHMWeaponComponent.h"
#include "SHMGameplayTags.h"

USHMWeaponComponent::USHMWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USHMWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	// 默认双武器：主剑副弓（数值先拍脑袋，调平衡是第五次开工的事）。
	// 在 C++ 里配齐是刻意的——弓是画像分化的输入源，不能依赖蓝图有没有人去配。
	SetMainWeapon(SHMTags::Weapon_Sword.GetTag(), EAttackPattern::Melee_Sweep, 20.f, 0.4f, 200.f);
	SetSubWeapon (SHMTags::Weapon_Bow.GetTag(),   EAttackPattern::Ranged_Shot, 12.f, 0.6f, 3000.f);
}

void USHMWeaponComponent::SetMainWeapon(FGameplayTag Tag, EAttackPattern Pattern, float Damage, float Speed, float Range)
{
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
	// 副武器未配置时是安全空操作——不切到无效武器
	// （上层 OnSwitchWeapon 靠"切换前后 Tag 是否变化"决定要不要广播，空操作不会误报）
	if (!SubWeaponTag.IsValid())
	{
		return;
	}

	bMainWeaponActive = !bMainWeaponActive;

	if (bMainWeaponActive)
	{
		ActiveWeaponTag     = MainWeaponTag;
		ActiveAttackPattern = MainWeaponPattern;
		ActiveBaseDamage    = MainWeaponDamage;
		ActiveAttackSpeed   = MainWeaponSpeed;
	}
	else
	{
		ActiveWeaponTag     = SubWeaponTag;
		ActiveAttackPattern = SubWeaponPattern;
		ActiveBaseDamage    = SubWeaponDamage;
		ActiveAttackSpeed   = SubWeaponSpeed;
	}
}
