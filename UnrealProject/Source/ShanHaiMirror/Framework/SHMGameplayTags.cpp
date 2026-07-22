#include "SHMGameplayTags.h"

namespace SHMTags
{
	// --- 战斗事件 ---
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Attack,       "Event.Combat.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_DamageDealt,  "Event.Combat.DamageDealt");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_HitTaken,     "Event.Combat.HitTaken");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Kill,         "Event.Combat.Kill");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Dodge,        "Event.Combat.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_SkillCast,    "Event.Combat.SkillCast");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_WeaponSwitch, "Event.Combat.WeaponSwitch");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_LowHp,        "Event.Combat.LowHp");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Death,        "Event.Combat.Death");

	// --- 流程事件 ---
	UE_DEFINE_GAMEPLAY_TAG(Event_Flow_RoomFinished,   "Event.Flow.RoomFinished");
	UE_DEFINE_GAMEPLAY_TAG(Event_Flow_FloorFinished,  "Event.Flow.FloorFinished");

	// --- 道具事件 ---
	UE_DEFINE_GAMEPLAY_TAG(Event_Item_Use,            "Event.Item.Use");

	// --- 武器 ---
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Sword,              "Weapon.Sword");
	UE_DEFINE_GAMEPLAY_TAG(Weapon_Bow,                "Weapon.Bow");

	// --- 敌人原型 ---
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Grunt,               "Enemy.Grunt");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Tank,                "Enemy.Tank");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Rush,                "Enemy.Rush");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Shooter,             "Enemy.Shooter");

	// --- 玩家原型 ---
	UE_DEFINE_GAMEPLAY_TAG(Archetype_Ranger,          "Archetype.Ranger");
	UE_DEFINE_GAMEPLAY_TAG(Archetype_Vanguard,        "Archetype.Vanguard");
	UE_DEFINE_GAMEPLAY_TAG(Archetype_Berserker,       "Archetype.Berserker");
	UE_DEFINE_GAMEPLAY_TAG(Archetype_Survivor,        "Archetype.Survivor");

	// --- Build 流派 ---
	UE_DEFINE_GAMEPLAY_TAG(Build_Melee,               "Build.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Build_Ranged,              "Build.Ranged");

	// --- 能力 ---
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge,             "Ability.Dodge");
}
