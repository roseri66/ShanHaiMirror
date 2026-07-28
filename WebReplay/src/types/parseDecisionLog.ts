// ============================================================================
// 决策日志解析器
//
// 设计前提：**前端对 JSON 同样不该信任。**
// 这不是防御性编程的口号，是这个项目自己的主张的延伸——UE 侧「不信任 LLM 输出」
// 用四道护栏兜住；这里「不信任日志文件」用同一个态度兜住。日志可能来自旧版本、
// 可能被手改过、可能是别人拖进来的完全无关的 JSON。
//
// 两条规矩：
//   ① **一次收集全部问题**，不是抛第一个就停。排查一个坏文件时，
//      「这 5 个字段有问题」比「第 3 行有问题（修完再跑，发现第 9 行也有）」省一半来回。
//   ② **能降级就不报错**。未知枚举、缺可选字段 → 记一条警告继续渲染；
//      只有「根本不是这个格式」才是致命错误。
// ============================================================================

import {
  SCHEMA_VERSION,
  normalizeGuard,
  type AvailableRule,
  type Decision,
  type DecisionRun,
  type Floor,
  type Intent,
  type Profile,
  type RuleIntent,
  type RuleModifier,
  type Trace,
  type Validation,
  type Violation,
} from './decisionLog'

export type ParseResult =
  | { ok: true; run: DecisionRun; warnings: string[] }
  | { ok: false; errors: string[]; warnings: string[] }

/** 解析过程中的问题收集器。致命的进 errors，可降级的进 warnings。 */
class Issues {
  readonly errors: string[] = []
  readonly warnings: string[] = []

  error(msg: string) {
    this.errors.push(msg)
  }

  warn(msg: string) {
    this.warnings.push(msg)
  }
}

// --- 取值助手：拿不到就用默认值 + 记一条警告，永不抛 -------------------------

function isPlainObject(v: unknown): v is Record<string, unknown> {
  return typeof v === 'object' && v !== null && !Array.isArray(v)
}

function pickNumber(
  obj: Record<string, unknown>,
  key: string,
  fallback: number,
  where: string,
  issues: Issues,
): number {
  const v = obj[key]
  if (typeof v === 'number' && Number.isFinite(v)) return v
  if (v !== undefined) {
    issues.warn(`${where}.${key} 不是有效数字（实际 ${JSON.stringify(v)}），按 ${fallback} 处理`)
  }
  return fallback
}

function pickString(
  obj: Record<string, unknown>,
  key: string,
  fallback: string,
  where: string,
  issues: Issues,
): string {
  const v = obj[key]
  if (typeof v === 'string') return v
  if (v !== undefined) {
    issues.warn(`${where}.${key} 不是字符串（实际 ${JSON.stringify(v)}），按 "${fallback}" 处理`)
  }
  return fallback
}

function pickBool(
  obj: Record<string, unknown>,
  key: string,
  fallback: boolean,
  where: string,
  issues: Issues,
): boolean {
  const v = obj[key]
  if (typeof v === 'boolean') return v
  if (v !== undefined) {
    issues.warn(`${where}.${key} 不是布尔值（实际 ${JSON.stringify(v)}），按 ${fallback} 处理`)
  }
  return fallback
}

function pickObject(
  obj: Record<string, unknown>,
  key: string,
  where: string,
  issues: Issues,
): Record<string, unknown> {
  const v = obj[key]
  if (isPlainObject(v)) return v
  if (v !== undefined) {
    issues.warn(`${where}.${key} 不是对象，按空对象处理`)
  }
  return {}
}

function pickArray(
  obj: Record<string, unknown>,
  key: string,
  where: string,
  issues: Issues,
): unknown[] {
  const v = obj[key]
  if (Array.isArray(v)) return v
  if (v !== undefined) {
    issues.warn(`${where}.${key} 不是数组，按空数组处理`)
  }
  return []
}

// --- 各段解析 ---------------------------------------------------------------

/** 标签 → 权重。非数字的条目跳过，其余照常——与 UE 侧 FSHMJsonIntent 的宽容度一致。 */
function parseWeights(
  raw: Record<string, unknown>,
  where: string,
  issues: Issues,
): Record<string, number> {
  const out: Record<string, number> = {}
  for (const [tag, value] of Object.entries(raw)) {
    if (typeof value === 'number' && Number.isFinite(value)) {
      out[tag] = value
    } else {
      issues.warn(`${where}.${tag} 权重不是有效数字，已跳过`)
    }
  }
  return out
}

function parseRuleIntents(raw: unknown[], where: string, issues: Issues): RuleIntent[] {
  const out: RuleIntent[] = []
  raw.forEach((item, i) => {
    if (!isPlainObject(item)) {
      issues.warn(`${where}[${i}] 不是对象，已跳过`)
      return
    }
    out.push({
      tag: pickString(item, 'tag', '', `${where}[${i}]`, issues),
      level: pickString(item, 'level', '', `${where}[${i}]`, issues),
    })
  })
  return out
}

function parseRuleModifiers(raw: unknown[], where: string, issues: Issues): RuleModifier[] {
  const out: RuleModifier[] = []
  raw.forEach((item, i) => {
    if (!isPlainObject(item)) {
      issues.warn(`${where}[${i}] 不是对象，已跳过`)
      return
    }
    const at = `${where}[${i}]`
    out.push({
      tag: pickString(item, 'tag', '', at, issues),
      level: pickString(item, 'level', '', at, issues),
      multiplier: pickNumber(item, 'multiplier', 1, at, issues),
      cost: pickNumber(item, 'cost', 0, at, issues),
    })
  })
  return out
}

function parseAvailableRules(raw: unknown[], where: string, issues: Issues): AvailableRule[] {
  const out: AvailableRule[] = []
  raw.forEach((item, i) => {
    if (!isPlainObject(item)) {
      issues.warn(`${where}[${i}] 不是对象，已跳过`)
      return
    }
    const at = `${where}[${i}]`
    out.push({
      tag: pickString(item, 'tag', '', at, issues),
      level: pickString(item, 'level', '', at, issues),
      cost: pickNumber(item, 'cost', 0, at, issues),
    })
  })
  return out
}

/** 画像。缺项按 0；`confidence` 缺失按 0.5（中性），与设计文档的映射表一致。 */
function parseProfile(raw: Record<string, unknown>, where: string, issues: Issues): Profile {
  return {
    buildConcentration: pickNumber(raw, 'buildConcentration', 0, where, issues),
    combatEfficiency: pickNumber(raw, 'combatEfficiency', 0, where, issues),
    resourceSurplus: pickNumber(raw, 'resourceSurplus', 0, where, issues),
    strategySwitch: pickNumber(raw, 'strategySwitch', 0, where, issues),
    survivalPressure: pickNumber(raw, 'survivalPressure', 0, where, issues),
    confidence: pickNumber(raw, 'confidence', 0.5, where, issues),
    dominantArchetype: pickString(raw, 'dominantArchetype', 'None', where, issues),
  }
}

function parseIntent(raw: Record<string, unknown>, where: string, issues: Issues): Intent {
  return {
    // 未知的 challengeLevel 原样透传：UE 侧新增等级时页面该显示原值而不是崩
    challengeLevel: pickString(raw, 'challengeLevel', '', where, issues),
    enemyWeights: parseWeights(
      pickObject(raw, 'enemyWeights', where, issues),
      `${where}.enemyWeights`,
      issues,
    ),
    ruleIntents: parseRuleIntents(
      pickArray(raw, 'ruleIntents', where, issues),
      `${where}.ruleIntents`,
      issues,
    ),
    narration: pickString(raw, 'narration', '', where, issues),
    reason: pickString(raw, 'reason', '', where, issues),
  }
}

function parseDecision(raw: Record<string, unknown>, where: string, issues: Issues): Decision {
  return {
    challengeLevel: pickString(raw, 'challengeLevel', '', where, issues),
    enemyWeights: parseWeights(
      pickObject(raw, 'enemyWeights', where, issues),
      `${where}.enemyWeights`,
      issues,
    ),
    ruleModifiers: parseRuleModifiers(
      pickArray(raw, 'ruleModifiers', where, issues),
      `${where}.ruleModifiers`,
      issues,
    ),
    narration: pickString(raw, 'narration', '', where, issues),
    reason: pickString(raw, 'reason', '', where, issues),
  }
}

function parseValidation(raw: Record<string, unknown>, where: string, issues: Issues): Validation {
  const violations: Violation[] = []
  pickArray(raw, 'violations', where, issues).forEach((item, i) => {
    if (!isPlainObject(item)) {
      issues.warn(`${where}.violations[${i}] 不是对象，已跳过`)
      return
    }
    const at = `${where}.violations[${i}]`
    const rawGuard = pickString(item, 'guard', '', at, issues)
    violations.push({
      // 归一化掉 "ESHMGuardrail::" 前缀，但**不做白名单校验**——
      // UE 侧加第五道护栏时，这里该显示一盏灰灯写着新名字，而不是把这条违规吞掉
      guard: normalizeGuard(rawGuard),
      rawGuard,
      detail: pickString(item, 'detail', '', at, issues),
    })
  })

  return {
    valid: pickBool(raw, 'valid', violations.length === 0, where, issues),
    violations,
  }
}

function parseTrace(raw: Record<string, unknown>, where: string, issues: Issues): Trace {
  return {
    providerId: pickString(raw, 'providerId', '', where, issues),
    degraded: pickBool(raw, 'degraded', false, where, issues),
    degradeReason: pickString(raw, 'degradeReason', '', where, issues),
    elapsedMs: pickNumber(raw, 'elapsedMs', 0, where, issues),
  }
}

/**
 * 解析一层。
 * 返回 null 表示这一层根本不可用（不是对象 / 无层号），跳过它但不影响其余层——
 * 一层坏掉不该让整份日志打不开。
 */
function parseFloor(raw: unknown, index: number, issues: Issues): Floor | null {
  const where = `floors[${index}]`

  if (!isPlainObject(raw)) {
    issues.warn(`${where} 不是对象，已跳过这一层`)
    return null
  }

  if (typeof raw.floorIndex !== 'number') {
    issues.warn(`${where} 缺 floorIndex，已跳过这一层`)
    return null
  }

  return {
    floorIndex: raw.floorIndex,
    // snapshot 缺失是**正常的**：契约里声明了，实测导出里没有。不记警告，免得每份日志都刷屏。
    snapshot: raw.snapshot,
    profile: parseProfile(pickObject(raw, 'profile', where, issues), `${where}.profile`, issues),
    context: (() => {
      const ctx = pickObject(raw, 'context', where, issues)
      return {
        challengeBudget: pickNumber(ctx, 'challengeBudget', 0, `${where}.context`, issues),
        availableRules: parseAvailableRules(
          pickArray(ctx, 'availableRules', `${where}.context`, issues),
          `${where}.context.availableRules`,
          issues,
        ),
      }
    })(),
    rawIntent: parseIntent(
      pickObject(raw, 'rawIntent', where, issues),
      `${where}.rawIntent`,
      issues,
    ),
    validation: parseValidation(
      pickObject(raw, 'validation', where, issues),
      `${where}.validation`,
      issues,
    ),
    decision: parseDecision(
      pickObject(raw, 'decision', where, issues),
      `${where}.decision`,
      issues,
    ),
    trace: parseTrace(pickObject(raw, 'trace', where, issues), `${where}.trace`, issues),
  }
}

// --- 入口 -------------------------------------------------------------------

/**
 * 把任意来源的数据解析成 DecisionRun。
 *
 * 致命错误（返回 ok:false）只有四种：不是对象、缺 schemaVersion、版本不匹配、没有可用的层。
 * 其余一切问题都降级成警告，页面照常渲染。
 */
export function parseDecisionLog(raw: unknown): ParseResult {
  const issues = new Issues()

  if (!isPlainObject(raw)) {
    issues.error('顶层不是一个 JSON 对象——这不像是决策日志文件')
    return { ok: false, errors: issues.errors, warnings: issues.warnings }
  }

  // --- 版本闸门：先于一切字段检查 ---
  // 版本不符时**不静默渲染**。字段名可能同名而语义已变，那种错误比打不开更难查。
  const version = raw.schemaVersion
  if (typeof version !== 'number') {
    issues.error(
      `缺少 schemaVersion 字段（本回放器只认版本 ${SCHEMA_VERSION}）——` +
        '确认这是 SHM.ExportDecisionLog 导出的决策日志吗？',
    )
    return { ok: false, errors: issues.errors, warnings: issues.warnings }
  }
  if (version !== SCHEMA_VERSION) {
    issues.error(
      `决策日志版本为 ${version}，本回放器只支持版本 ${SCHEMA_VERSION}。` +
        '格式已经演进，字段语义可能不同，拒绝解析以免显示出错误的结论。',
    )
    return { ok: false, errors: issues.errors, warnings: issues.warnings }
  }

  const rawFloors = raw.floors
  if (!Array.isArray(rawFloors)) {
    issues.error('缺少 floors 数组——没有层数据就没什么可回放的')
    return { ok: false, errors: issues.errors, warnings: issues.warnings }
  }

  const floors: Floor[] = []
  rawFloors.forEach((item, i) => {
    const floor = parseFloor(item, i, issues)
    if (floor) floors.push(floor)
  })

  if (floors.length === 0) {
    issues.error(
      rawFloors.length === 0
        ? 'floors 数组是空的——这一局没有记录任何决策'
        : `floors 里 ${rawFloors.length} 层全部无法解析（详见警告）`,
    )
    return { ok: false, errors: issues.errors, warnings: issues.warnings }
  }

  // 按层号排序：导出顺序理应就是层序，但排一下不花钱，而乱序会让时间轴撒谎
  floors.sort((a, b) => a.floorIndex - b.floorIndex)

  const run: DecisionRun = {
    schemaVersion: version,
    runId: pickString(raw, 'runId', '', '顶层', issues),
    startedAt: pickString(raw, 'startedAt', '', '顶层', issues),
    // totalFloors 缺失时用实际层数兜底，比显示 0 诚实
    totalFloors: pickNumber(raw, 'totalFloors', floors.length, '顶层', issues),
    floors,
  }

  return { ok: true, run, warnings: issues.warnings }
}
