// ============================================================================
// 本局统计与格式化的测试
//
// 这两样为什么值得单独测：
//   · 统计数字会印在页面顶部，面试官会拿它跟时间轴上的卡片对。**数错了比不显示更糟。**
//   · 格式化规则里有几条反直觉的决定（0ms 显示破折号、平均耗时排除观察层），
//     用测试把这些决定固定住，免得日后被"顺手改得更自然"改回去。
//
// 本文件先于实现写成（M1 起恢复 TDD）。
// ============================================================================

import { describe, expect, it } from 'vitest'
import {
  computeRunStats,
  degradeKind,
  formatElapsed,
  formatMultiplier,
  formatPercent,
  narrationPair,
} from './runStats'
import { parseDecisionLog } from './parseDecisionLog'
import type { DecisionRun } from './decisionLog'
import greenSample from '../../../Docs/samples/DecisionLog_Sample.json'
import guardrailRun from '../../../Docs/samples/DecisionLog_Guardrail_Run.json'
import guardrailIdeal from '../../../Docs/samples/DecisionLog_Guardrail_Ideal.json'

function runOf(raw: unknown): DecisionRun {
  const r = parseDecisionLog(raw)
  if (!r.ok) throw new Error(r.errors.join('\n'))
  return r.run
}

// ---------------------------------------------------------------------------
describe('computeRunStats · 真实样例（数字须与手算一致）', () => {
  it('全绿样例：F0 观察 + F1/F2 由 LLM 直采，零拦截零降级', () => {
    const s = computeRunStats(runOf(greenSample))

    expect(s.totalFloors).toBe(3)
    expect(s.llmDirect).toBe(2) // F1、F2 两层 providerId=Llm 且未降级
    expect(s.degraded).toBe(0)
    expect(s.violations).toBe(0)
    expect(s.byGuard).toEqual({ Schema: 0, Budget: 0, Conflict: 0, Fairness: 0 })
  })

  it('全绿样例：平均耗时排除观察层（F0 的 0ms 不该拉低平均）', () => {
    const s = computeRunStats(runOf(greenSample))

    // F1=3721.015625ms、F2=2804.269775390625ms，F0=0ms 不计入
    expect(s.timedFloors).toBe(2)
    expect(s.avgElapsedMs).toBeCloseTo((3721.015625 + 2804.269775390625) / 2, 3)
  })

  it('理想夹具：三层被拦、三层降级，分道各一次', () => {
    const s = computeRunStats(runOf(guardrailIdeal))

    // 四层。**这不是一局游戏**——F3 在真实对局里到不了（TotalFloors=3），
    // 这份是控制台逐层调出来的夹具，存在的意义只是证明三道护栏结构上都拦得住。
    expect(s.totalFloors).toBe(4)
    expect(s.llmDirect).toBe(0) // 没走 LLM，Intent 全由回放脚本注入
    expect(s.degraded).toBe(3)
    expect(s.violations).toBe(3)
    expect(s.byGuard).toEqual({ Schema: 1, Budget: 1, Conflict: 1, Fairness: 0 })
  })

  it('真实对局：三层形状，拦 2 次降 2 次', () => {
    const s = computeRunStats(runOf(guardrailRun))

    expect(s.totalFloors).toBe(3)
    expect(s.llmDirect).toBe(0)
    expect(s.degraded).toBe(2)
    expect(s.violations).toBe(2)
    // 真实一局里只有 F1/F2 会走 Provider，所以最多两次拦截——
    // 三道全亮是夹具才做得到的理想情况
    expect(s.byGuard).toEqual({ Schema: 0, Budget: 1, Conflict: 1, Fairness: 0 })
  })

  it('理想夹具：回放是同步的，三层耗时都在亚毫秒级', () => {
    const s = computeRunStats(runOf(guardrailIdeal))
    // F0 观察层 0ms 不计入；F1/F2/F3 走回放 Provider，读内存没有等待
    expect(s.timedFloors).toBe(3)
    expect(s.avgElapsedMs).toBeGreaterThan(0)
    expect(s.avgElapsedMs).toBeLessThan(1)
  })

  it('全部耗时为 0 时平均耗时不产生 NaN', () => {
    const s = computeRunStats(
      runOf({
        schemaVersion: 1,
        floors: [{ floorIndex: 0, trace: { providerId: 'ObserveFloor', elapsedMs: 0 } }],
      }),
    )
    expect(s.timedFloors).toBe(0)
    expect(s.avgElapsedMs).toBe(0)
    expect(Number.isNaN(s.avgElapsedMs)).toBe(false)
  })
})

// ---------------------------------------------------------------------------
describe('computeRunStats · 边界', () => {
  it('降级的 LLM 层不算"直采"', () => {
    const s = computeRunStats(
      runOf({
        schemaVersion: 1,
        floors: [
          { floorIndex: 0, trace: { providerId: 'Llm', degraded: false, elapsedMs: 100 } },
          { floorIndex: 1, trace: { providerId: 'Llm', degraded: true, elapsedMs: 200 } },
        ],
      }),
    )
    expect(s.llmDirect).toBe(1)
    expect(s.degraded).toBe(1)
  })

  it('同一层多条违规分别计入各自的道', () => {
    const s = computeRunStats(
      runOf({
        schemaVersion: 1,
        floors: [
          {
            floorIndex: 0,
            validation: {
              valid: false,
              violations: [
                { guard: 'Schema', detail: 'a' },
                { guard: 'Schema', detail: 'b' },
                { guard: 'Budget', detail: 'c' },
              ],
            },
          },
        ],
      }),
    )
    expect(s.violations).toBe(3)
    expect(s.byGuard.Schema).toBe(2)
    expect(s.byGuard.Budget).toBe(1)
  })

  it('未知护栏计入总数但不污染四道的计数', () => {
    const s = computeRunStats(
      runOf({
        schemaVersion: 1,
        floors: [
          {
            floorIndex: 0,
            validation: {
              valid: false,
              violations: [{ guard: 'ESHMGuardrail::Telemetry', detail: '第五道' }],
            },
          },
        ],
      }),
    )
    expect(s.violations).toBe(1)
    expect(s.byGuard).toEqual({ Schema: 0, Budget: 0, Conflict: 0, Fairness: 0 })
    expect(s.unknownGuards).toEqual({ Telemetry: 1 })
  })
})

// ---------------------------------------------------------------------------
describe('degradeKind · 两种降级的 rawIntent 含义不同，必须分开', () => {
  it('未降级 → none', () => {
    const run = runOf(greenSample)
    expect(run.floors.every((f) => degradeKind(f) === 'none')).toBe(true)
  })

  it('护栏拒绝（降级②）→ rejected，rawIntent 是被拒的原件', () => {
    const run = runOf(guardrailIdeal)
    for (const idx of [1, 2, 3]) {
      const f = run.floors.find((x) => x.floorIndex === idx)!
      expect(degradeKind(f)).toBe('rejected')
    }
  })

  it('Provider 交不出结果（降级①）→ noOutput，rawIntent 已经是本地的', () => {
    // DirectorCore 在这条路径上直接拿本地 Intent 去走 FinishDecision，
    // 所以日志里的 rawIntent 并不是"被拦下的那份"——没有原件可对照。
    const run = runOf({
      schemaVersion: 1,
      floors: [
        {
          floorIndex: 1,
          validation: { valid: true, violations: [] },
          trace: { providerId: 'Llm', degraded: true, degradeReason: 'Llm 无可用输出' },
        },
      ],
    })
    expect(degradeKind(run.floors[0])).toBe('noOutput')
  })
})

// ---------------------------------------------------------------------------
describe('narrationPair · 台词在降级后会被换成本地库的', () => {
  it('未降级时前后台词一致，只需展示一句', () => {
    const run = runOf(greenSample)
    const f1 = run.floors.find((x) => x.floorIndex === 1)!
    const pair = narrationPair(f1)
    expect(pair.actual).toBe(f1.decision.narration)
    expect(pair.rejected).toBeUndefined()
  })

  it('护栏拒绝后，被拦的台词与实际播出的台词都要留下', () => {
    const run = runOf(guardrailIdeal)
    const f1 = run.floors.find((x) => x.floorIndex === 1)!
    const pair = narrationPair(f1)

    expect(pair.rejected).toBe('你的箭袋会更浅，你的箭也会更钝。')
    expect(pair.actual).toBe('你的弓用得很好。但这一层，别指望站在原地。')
    expect(pair.actual).not.toBe(pair.rejected)
  })

  it('真实对局里同样如此，且实际台词与挑战等级对得上', () => {
    const run = runOf(guardrailRun)
    const f1 = run.floors.find((x) => x.floorIndex === 1)!
    const pair = narrationPair(f1)

    expect(pair.rejected).toBe('你的箭袋会更浅，你的箭也会更钝。')
    // 本地库按 ChallengeLevel 选句：F1 判定为 Pressure，对应这一句
    // （SHMLocalProvider.cpp:204）。夹具因为画像写死成高置信度，判到了 Counter，
    // 所以两份样例同一层的实际台词不同——这不是矛盾，是输入不同。
    expect(f1.decision.challengeLevel).toBe('Pressure')
    expect(pair.actual).toBe('你走得太顺了。镜中的试炼，该加深了。')
  })

  it('观察层 rawIntent 为空，不该显示一句空的"被拦台词"', () => {
    // F0 压根不调用 Provider，rawIntent.narration 是空串
    const run = runOf(guardrailIdeal)
    const f0 = run.floors.find((x) => x.floorIndex === 0)!
    expect(narrationPair(f0).rejected).toBeUndefined()
    expect(narrationPair(f0).actual).toBe('第一层。我只是在看。')
  })
})

// ---------------------------------------------------------------------------
describe('formatElapsed', () => {
  it('0 显示破折号而不是 "0ms"', () => {
    // 观察层压根没调用 Provider，耗时字段是 0。显示 "0ms" 会让人以为测到了一次极快的调用
    expect(formatElapsed(0)).toBe('—')
  })

  it('亚毫秒显示 "<1ms" 而不是四舍五入成 "0ms"', () => {
    // 回放 Provider 读内存，实测 0.005~0.012ms。
    // 若也渲染成 "0ms"，就与观察层的"根本没跑"混为一谈——
    // 「没执行」和「执行了但极快」必须能区分，这和 UE 侧"失败必须出声"是同一条纪律
    expect(formatElapsed(0.005401670932769775)).toBe('<1ms')
    expect(formatElapsed(0.9)).toBe('<1ms')
  })

  it('1 秒以下用毫秒且取整', () => {
    expect(formatElapsed(372)).toBe('372ms')
    expect(formatElapsed(372.8)).toBe('373ms')
    expect(formatElapsed(999)).toBe('999ms')
  })

  it('1 秒及以上用秒，保留一位', () => {
    expect(formatElapsed(1000)).toBe('1.0s')
    expect(formatElapsed(3721.015625)).toBe('3.7s')
    expect(formatElapsed(5004)).toBe('5.0s')
  })
})

// ---------------------------------------------------------------------------
describe('formatPercent / formatMultiplier', () => {
  it('权重转百分比，抹掉 float32 转 double 的尾巴', () => {
    // 这些值来自真实日志：UE 的 float 落到 JSON 里就是这个样子
    expect(formatPercent(0.550000011920929)).toBe('55%')
    expect(formatPercent(0.20000000298023224)).toBe('20%')
    expect(formatPercent(0)).toBe('0%')
    expect(formatPercent(1)).toBe('100%')
  })

  it('倍率保留两位并带乘号', () => {
    expect(formatMultiplier(0.800000011920929)).toBe('×0.80')
    expect(formatMultiplier(1.149999976158142)).toBe('×1.15')
    expect(formatMultiplier(1)).toBe('×1.00')
  })
})
