// ============================================================================
// 本局统计 + 展示用格式化
//
// 全部由 floors 现算，**不依赖日志里任何新增字段**——UE 侧不必为了这个页面
// 多导出一个数。统计口径写在这里而不是散在组件里，是因为页面顶部的数字
// 会被拿去跟时间轴上的卡片一一核对，算错比不显示更糟。
// ============================================================================

import { GUARD_ORDER, normalizeGuard, type DecisionRun, type Guard } from './decisionLog'

export interface RunStats {
  totalFloors: number
  /** LLM 直采：providerId 是 Llm **且没有降级**。降级了就不算它的功劳。 */
  llmDirect: number
  degraded: number
  /** 拦截总次数（同一层多条违规分别计数）。 */
  violations: number
  /** 四道护栏各自拦了几次。四个 key 恒定存在，值为 0 时页面显示灰灯而不是不显示。 */
  byGuard: Record<Guard, number>
  /** 不在已知四道之内的护栏。UE 侧加了第五道而前端没跟上时，数据不会被吞掉。 */
  unknownGuards: Record<string, number>
  /** 真正调用过 Provider 的层数（elapsedMs > 0）。 */
  timedFloors: number
  /** 平均耗时，只对 timedFloors 求平均。 */
  avgElapsedMs: number
}

export function computeRunStats(run: DecisionRun): RunStats {
  const byGuard = Object.fromEntries(GUARD_ORDER.map((g) => [g, 0])) as Record<Guard, number>
  const unknownGuards: Record<string, number> = {}

  let llmDirect = 0
  let degraded = 0
  let violations = 0
  let elapsedSum = 0
  let timedFloors = 0

  for (const floor of run.floors) {
    if (floor.trace.degraded) {
      degraded++
    } else if (floor.trace.providerId === 'Llm') {
      llmDirect++
    }

    for (const v of floor.validation.violations) {
      violations++
      const guard = normalizeGuard(v.guard)
      if (guard in byGuard) {
        byGuard[guard as Guard]++
      } else {
        unknownGuards[guard] = (unknownGuards[guard] ?? 0) + 1
      }
    }

    // 观察层（ObserveFloor）和导演关闭层压根没调用 Provider，耗时是 0。
    // 把它们算进平均会凭空拉低数字——那是在给自己脸上贴金，不做。
    if (floor.trace.elapsedMs > 0) {
      elapsedSum += floor.trace.elapsedMs
      timedFloors++
    }
  }

  return {
    totalFloors: run.floors.length,
    llmDirect,
    degraded,
    violations,
    byGuard,
    unknownGuards,
    timedFloors,
    // 除零保护：一局全是观察层/回放层时 timedFloors 可能为 0
    avgElapsedMs: timedFloors > 0 ? elapsedSum / timedFloors : 0,
  }
}

// --- 降级的两种含义 ---------------------------------------------------------

/**
 * 降级类型。
 *
 * **两种降级下 `rawIntent` 的含义完全不同**，前端不能一看 `degraded` 就当成
 * "这是 LLM 想干的"：
 *
 *   `rejected` 护栏拒绝（降级②）——`rawIntent` 是**被拒的原件**，
 *              和 `decision` 并排看才有意义，是这个页面的全部价值所在。
 *   `noOutput` Provider 交不出结果（降级①，超时/HTTP 错/脚本缺层）——
 *              DirectorCore 直接拿本地 Intent 去走后续流程（SHMDirectorCore.cpp:304），
 *              所以日志里的 `rawIntent` **已经是本地的了**，没有原件可对照。
 *
 * 两者靠 `violations` 是否为空区分：护栏拒绝必定留下违规记录，交不出结果则不会。
 */
export type DegradeKind = 'none' | 'rejected' | 'noOutput'

export function degradeKind(floor: {
  trace: { degraded: boolean }
  validation: { violations: unknown[] }
}): DegradeKind {
  if (!floor.trace.degraded) return 'none'
  return floor.validation.violations.length > 0 ? 'rejected' : 'noOutput'
}

/**
 * 台词的前后对照。
 *
 * 降级后**台词也会被换成本地库的**——`Decision.NarrationLine` 取自
 * `EffectiveIntent`，而护栏拒绝时 `EffectiveIntent` 已经是本地重新决策的结果。
 * 实测护栏样例三层的 `decision.narration` 完全相同（本地 Provider 对同一画像
 * 产出同一句），而 `rawIntent.narration` 三层各异。
 *
 * 所以只显示一句是错的：
 *   · 只显示 `decision` → 把本地生成的句子当成了 LLM 说的话
 *   · 只显示 `rawIntent` → 显示了一句玩家其实没听到的话
 * 两句都留，且**只在确实不同时才并排**——它们的差异本身就是"护栏连台词一起换掉了"
 * 的证据。
 */
export function narrationPair(floor: {
  rawIntent: { narration: string }
  decision: { narration: string }
}): { actual: string; rejected?: string } {
  const actual = floor.decision.narration
  const raw = floor.rawIntent.narration
  // 空串意味着这一层压根没走 Provider（观察层 / 导演关闭），没有"被拦的那句"
  if (!raw || raw === actual) return { actual }
  return { actual, rejected: raw }
}

// --- 格式化 -----------------------------------------------------------------
//
// 只影响**展示**。原始 JSON 一律不改——回放靠原值复现，
// 提前 round 会让"页面上看到的"和"实际喂回引擎的"悄悄分家。

/**
 * 耗时。
 *
 * 三档而不是两档，是为了区分两种不同的 0：
 *   `0`      → `—`      根本没调用 Provider（观察层）
 *   `(0,1)`  → `<1ms`   调用了，但是同步的（回放/本地 Provider）
 *   其余     → `372ms` / `3.7s`
 * 把后两者都渲染成 "0ms" 会让"没执行"和"执行了但极快"无法分辨。
 */
export function formatElapsed(ms: number): string {
  if (!Number.isFinite(ms) || ms <= 0) return '—'
  if (ms < 1) return '<1ms'
  if (ms < 1000) return `${Math.round(ms)}ms`
  return `${(ms / 1000).toFixed(1)}s`
}

/**
 * 权重 → 百分比。
 * UE 的 float 落进 JSON 会带一串尾巴（`0.550000011920929`），
 * 那是 float32→double 的产物，不是精度信息，展示时直接抹掉。
 */
export function formatPercent(v: number): string {
  if (!Number.isFinite(v)) return '—'
  return `${Math.round(v * 100)}%`
}

/** 倍率。保留两位——规则表里的值就是两位（0.85 / 1.15 / 0.70）。 */
export function formatMultiplier(v: number): string {
  if (!Number.isFinite(v)) return '—'
  return `×${v.toFixed(2)}`
}
