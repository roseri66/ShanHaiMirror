// ============================================================================
// 决策日志的 TypeScript 契约
//
// ⚠️ **字段名的唯一真源是 UE 侧的 C++ 头文件**：
//    UnrealProject/Source/ShanHaiMirror/Director/SHMDecisionLogFormat.h
//    本文件是它的 TS 镜像。**改那边必须同步改这边**，否则字段漂移会在运行时才炸，
//    而且炸得很安静（TryGet 系列取不到值只是留空，不报错）。
//
// 这份格式有四个消费方：ReplayProvider（读它复现决策）、本回放器（渲染它）、
// 可能的后端（落库）、以及人（读原始 JSON）。任何一方擅自改字段名都会拆掉其余三方。
// ============================================================================

/** 与 SHMLogFormat::SchemaVersion 对齐。C++ 那边改语义/删字段会 +1，这里必须跟上。 */
export const SCHEMA_VERSION = 1

// --- 护栏前：Provider（可能是 LLM）的原始输出 -------------------------------

/**
 * 规则意图。**只有 tag + level，没有任何数值字段——这是刻意的**（D-15）。
 * LLM 只在标签空间工作，数值要到第 ⑥ 步查表才产生。
 */
export interface RuleIntent {
  tag: string
  level: string
}

// --- 护栏后：查表解析出数值之后 ---------------------------------------------

/** 规则修改量。`multiplier` 是全链路唯一的数值产生点，它在这里第一次出现。 */
export interface RuleModifier {
  tag: string
  level: string
  multiplier: number
  cost: number
}

/** 候选规则。第 ③ 步 CONSTRAIN 已经把不安全的选项过滤掉了，Provider 只能在此集合内选。 */
export interface AvailableRule {
  tag: string
  level: string
  cost: number
}

// --- 画像 -------------------------------------------------------------------

/**
 * 五维画像 + 置信度。
 *
 * 注意 `confidence` 的量纲与其余五项不同（0–1 vs 0–100），
 * **不要把它塞进同一张雷达图**——那会让图撒谎。
 *
 * 另注：`resourceSurplus` 目前是中性占位。道具系统被 D-09 砍掉后它没有数据源，
 * LocalProvider 明确不消费它。所以画像**实际是四维 + 一个占位**。
 */
export interface Profile {
  buildConcentration: number
  combatEfficiency: number
  resourceSurplus: number
  strategySwitch: number
  survivalPressure: number
  confidence: number
  dominantArchetype: string
}

// --- Intent / Decision -------------------------------------------------------

/** 挑战等级。C++ 侧 EChallengeLevel，未知串在 UE 那边会回落 Stable。 */
export type ChallengeLevel =
  | 'Recovery'
  | 'Stable'
  | 'Pressure'
  | 'Counter'
  | 'Evolution'

/**
 * 护栏前的意图。
 * 与 Decision 的差别只有一处：`ruleIntents` 无数值，而 Decision 的 `ruleModifiers` 有。
 * **整个页面要让人看见的就是这一处差别。**
 */
export interface Intent {
  challengeLevel: string
  enemyWeights: Record<string, number>
  ruleIntents: RuleIntent[]
  narration: string
  reason: string
}

/** 护栏后的决策。注意它没有 `ruleIntents`，只有带数值的 `ruleModifiers`。 */
export interface Decision {
  challengeLevel: string
  enemyWeights: Record<string, number>
  ruleModifiers: RuleModifier[]
  narration: string
  reason: string
}

// --- 护栏 -------------------------------------------------------------------

/** 四道护栏。对应 C++ 侧 ESHMGuardrail。 */
export type Guard = 'Schema' | 'Budget' | 'Conflict' | 'Fairness'

/** 护栏灯带按这个顺序渲染，与 FSHMDecisionValidator::Validate() 的调用顺序一致。 */
export const GUARD_ORDER: readonly Guard[] = [
  'Schema',
  'Budget',
  'Conflict',
  'Fairness',
] as const

export interface Violation {
  /** 归一化后的护栏名。未知取值原样透传（UE 侧新增护栏而这里没跟上时不至于丢数据）。 */
  guard: Guard | string
  /** 归一化前的原值，用于排查契约漂移。与 `guard` 相同时说明没做过转换。 */
  rawGuard: string
  detail: string
}

/**
 * 把护栏名归一化成裸枚举名。
 *
 * UE 侧 `SHMDirectorCore.cpp` 用 `UEnum::GetValueAsString()` 序列化，
 * 实际导出的是 **`"ESHMGuardrail::Conflict"`** 而不是契约文档假设的 `"Conflict"`——
 * C++ 类型名漏进了一份三方共用的 JSON 契约。
 *
 * 正确的修法在 UE 侧（改用 `GetNameStringByValue`），但**这里也必须容错**：
 * 已经导出的日志改不了，而回放器要能打开旧文件。两种形式都接受，代价是三行代码。
 */
export function normalizeGuard(raw: string): string {
  const sep = raw.lastIndexOf('::')
  return sep >= 0 ? raw.slice(sep + 2) : raw
}

export interface Validation {
  valid: boolean
  violations: Violation[]
}

// --- 溯源 -------------------------------------------------------------------

/**
 * 谁出的决策、有没有降级、花了多久。
 * C++ 注释说得很直白：「事后无法补造，必须此刻填」。
 */
export interface Trace {
  /** Local | Llm | Replay | ObserveFloor | Disabled。未知值原样透传。 */
  providerId: string
  degraded: boolean
  degradeReason: string
  elapsedMs: number
}

// --- 层 / 局 ----------------------------------------------------------------

export interface Floor {
  floorIndex: number
  /**
   * ⚠️ 契约里声明了 `Key_Snapshot`，但**实测导出里没有这个字段**。
   * 所以它必须是可选的——把它当必填会让所有真实日志解析失败。
   * （这处不一致已记在设计文档里，UE 侧要么补导出、要么在 .h 注明「S1 暂不导出」。）
   */
  snapshot?: unknown
  profile: Profile
  context: {
    challengeBudget: number
    availableRules: AvailableRule[]
  }
  rawIntent: Intent
  validation: Validation
  decision: Decision
  trace: Trace
}

export interface DecisionRun {
  schemaVersion: number
  runId: string
  startedAt: string
  totalFloors: number
  floors: Floor[]
}
