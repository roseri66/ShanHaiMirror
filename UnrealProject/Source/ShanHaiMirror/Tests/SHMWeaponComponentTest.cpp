#include "Misc/AutomationTest.h"
#include "Framework/SHMWeaponComponent.h"
#include "Framework/SHMGameplayTags.h"
#include "Framework/SHMCoreTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// 武器切换测试 —— 画像分化的输入源，切换逻辑错了 AI 就永远只看到一种打法
// 组件可 NewObject 直测（不跑 BeginPlay，武器由测试显式设置）
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags WeaponTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;

	USHMWeaponComponent* MakeArmedComponent()
	{
		USHMWeaponComponent* Comp = NewObject<USHMWeaponComponent>(GetTransientPackage());
		Comp->SetMainWeapon(SHMTags::Weapon_Sword.GetTag(), EAttackPattern::Melee_Sweep, 20.f, 0.4f, 200.f);
		Comp->SetSubWeapon (SHMTags::Weapon_Bow.GetTag(),   EAttackPattern::Ranged_Shot, 12.f, 0.6f, 3000.f);
		return Comp;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMWeaponSwitchSwapsTest,
	"SHM.Combat.Weapon.Switch_SwapsActiveWeapon", WeaponTestFlags)
bool FSHMWeaponSwitchSwapsTest::RunTest(const FString& Parameters)
{
	USHMWeaponComponent* Comp = MakeArmedComponent();

	// 初始：主武器剑激活
	TestTrue(TEXT("初始激活主武器（剑）"), Comp->GetActiveWeaponTag() == SHMTags::Weapon_Sword.GetTag());

	// 切一次 → 弓，全套激活数据跟着换
	Comp->SwitchWeapon();
	TestTrue (TEXT("切换后激活弓"), Comp->GetActiveWeaponTag() == SHMTags::Weapon_Bow.GetTag());
	TestTrue (TEXT("攻击模式应为远程"), Comp->GetActiveAttackPattern() == EAttackPattern::Ranged_Shot);
	TestEqual(TEXT("伤害应为弓的数值"), Comp->GetActiveBaseDamage(), 12.f);
	TestFalse(TEXT("主武器不再激活"), Comp->IsMainWeaponActive());

	// 再切 → 回到剑
	Comp->SwitchWeapon();
	TestTrue (TEXT("再次切换回到剑"), Comp->GetActiveWeaponTag() == SHMTags::Weapon_Sword.GetTag());
	TestEqual(TEXT("伤害回到剑的数值"), Comp->GetActiveBaseDamage(), 20.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMWeaponSwitchNoSubTest,
	"SHM.Combat.Weapon.Switch_NoSubWeaponIsNoOp", WeaponTestFlags)
bool FSHMWeaponSwitchNoSubTest::RunTest(const FString& Parameters)
{
	// 只有主武器（副武器未配置）——切换应是安全空操作，不切到无效武器
	USHMWeaponComponent* Comp = NewObject<USHMWeaponComponent>(GetTransientPackage());
	Comp->SetMainWeapon(SHMTags::Weapon_Sword.GetTag(), EAttackPattern::Melee_Sweep, 20.f, 0.4f, 200.f);

	Comp->SwitchWeapon();

	TestTrue(TEXT("无副武器时切换后仍是剑"), Comp->GetActiveWeaponTag() == SHMTags::Weapon_Sword.GetTag());
	TestTrue(TEXT("主武器仍处于激活状态"), Comp->IsMainWeaponActive());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
