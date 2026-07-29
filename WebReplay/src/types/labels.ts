// ============================================================================
// 枚举 → 中文名
//
// ⚠️ **所有中文名的来源是 C++ 的 `UMETA(DisplayName=...)`**，逐字抄自：
//    Framework/SHMCoreTypes.h        （EChallengeLevel、EEnemyArchetype）
//    Director/SHMDirectorTypes.h     （ESHMGuardrail）
//    Framework/SHMGameplayTags.h     （Archetype.* 标签的注释）
//    C++ 那边改了 DisplayName，这里要跟着改，否则同一个枚举在游戏内和网页上叫两个名字。
//
// 统一走 `labelOf()` 而不是直接索引：**未知取值一律原样返回**，绝不返回 undefined。
// 页面上宁可显示一个没见过的英文标识符，也不能显示 "undefined" 或者整块崩掉。
// ============================================================================

/** 挑战等级。对齐 EChallengeLevel 的 UMETA。 */
export const CHALLENGE_LEVEL_LABELS: Record<string, string> = {
  Recovery: '恢复期(降压力)',
  Stable: '正常',
  Pressure: '施压',
  Counter: '明确反制',
  Evolution: '强制转型',
}

/** 四道护栏。对齐 ESHMGuardrail 的 UMETA。 */
export const GUARD_LABELS: Record<string, string> = {
  Schema: '结构合法性',
  Budget: '挑战预算',
  Conflict: '规则互斥',
  Fairness: '公平性',
}

/** 玩家原型。来自 SHMGameplayTags.h 里 Archetype.* 的注释。 */
export const ARCHETYPE_LABELS: Record<string, string> = {
  'Archetype.Ranger': '游侠',
  'Archetype.Vanguard': '先锋',
  'Archetype.Berserker': '狂战',
  'Archetype.Survivor': '幸存者',
  None: '未定',
  '': '未定',
}

/** 敌人原型。对齐 EEnemyArchetype 的 UMETA（只列 D-05 保留的 4 种）。 */
export const ENEMY_LABELS: Record<string, string> = {
  'Enemy.Grunt': '杂兵',
  'Enemy.Tank': '盾兵',
  'Enemy.Rush': '突进',
  'Enemy.Shooter': '射手',
}

/**
 * Provider 标识。
 * 这些不是 UMETA，是 `ISHMAIProvider::GetProviderName()` 的返回值
 * 加上 DirectorCore 里两个特殊分支（观察层 / 导演关闭）。
 */
export const PROVIDER_LABELS: Record<string, string> = {
  Llm: '大模型',
  Local: '本地规则',
  Replay: '回放脚本',
  ObserveFloor: '观察层',
  Disabled: '导演已关闭',
}

/** 规则等级。C++ 侧是裸字符串（见 FSHMDecisionValidator::IsLegalLevel）。 */
export const RULE_LEVEL_LABELS: Record<string, string> = {
  light: '轻度',
  medium: '中度',
  heavy: '重度',
}

/** 规则标签。CSV 的 RuleTag 列，见 UnrealProject/Data/RuleTable.csv。 */
export const RULE_TAG_LABELS: Record<string, string> = {
  'Rule.Ammo': '弹药',
  'Rule.Cooldown': '冷却',
  'Rule.Heal': '治疗',
  'Rule.MeleeDamage': '近战伤害',
  'Rule.RangedDamage': '远程伤害',
}

/**
 * 查表取中文名。**查不到就原样返回**——未知枚举是可能的（UE 侧先行新增），
 * 这时候页面该做的是老实显示原值，而不是崩、也不是显示 undefined。
 */
export function labelOf(table: Record<string, string>, key: string): string {
  return table[key] ?? key
}
