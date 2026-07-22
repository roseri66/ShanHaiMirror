#pragma once

#include "NativeGameplayTags.h"

// ============================================================================
// 原生 GameplayTag 声明
//
// 为什么用 native tag 而不是 FGameplayTag::RequestGameplayTag("...")：
//   ① 编译期存在性保证——拼错标签名编译不过，不用等到运行时 ensure
//   ② 不依赖 ini 注册顺序——native tag 由 C++ 静态注册，不会在 CDO 构造阶段查不到
//   ③ 无字符串哈希查找开销
//
// AI Director 的整个识别体系建立在标签上（TDD §2.4），标签写错是最难查的一类
// bug——它不会崩，只会让画像永远统计不到那把武器。这里用类型系统挡住。
// ============================================================================

namespace SHMTags
{
	// --- 战斗事件 ---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Attack);        // 发起一次攻击
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_DamageDealt);   // 造成伤害
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_HitTaken);      // 受击
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Kill);          // 击杀
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Dodge);         // 闪避
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_SkillCast);     // 释放技能
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_WeaponSwitch);  // 切换武器
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_LowHp);         // HP 跌破阈值
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Death);         // 死亡

	// --- 流程事件 ---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Flow_RoomFinished);    // 房间清空（Magnitude = 耗时秒）
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Flow_FloorFinished);   // 层结束

	// --- 道具事件 ---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Item_Use);             // 使用道具

	// --- 武器 ---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Sword);
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Bow);

	// --- 敌人原型（MVP 四原型，见 DECISIONS.md D-05）---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Grunt);
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Tank);
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Rush);
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Shooter);

	// --- 玩家原型（ProfileAnalyzer 输出）---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Archetype_Ranger);      // 游击：远程为主
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Archetype_Vanguard);    // 先锋：近战为主
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Archetype_Berserker);   // 狂战：近战 + 高风险
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Archetype_Survivor);    // 幸存：保守 + 高闪避

	// --- Build 流派标签（AI 识别打法的依据）---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Melee);
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Build_Ranged);

	// --- 能力 ---
	SHANHAIMIRROR_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dodge);
}
