// ============================================================================
// 解析器的回归网
//
// 为什么这一层值得测：它是**契约的守门人**。UE 侧改了 .h、导出格式变了、
// 别人拖进来一份别的 JSON——所有这些都在这里被挡住或降级。
// 而它是纯函数（无 DOM、无网络、无 Vue），测起来成本极低。
//
// 测试分三组，对应三种失败模式：
//   ① 真实数据能不能解析对（拿两份**真的从 UE 导出来的**样例跑）
//   ② 该失败的有没有失败（坏数据不能被静默渲染成错误结论）
//   ③ 该降级的有没有降级（字段缺失/未知枚举不能让整页崩掉）
// ============================================================================

import { describe, expect, it } from 'vitest'
import { parseDecisionLog } from './parseDecisionLog'
import { normalizeGuard, SCHEMA_VERSION } from './decisionLog'
import greenSample from '../../../Docs/samples/DecisionLog_Sample.json'
import guardrailSample from '../../../Docs/samples/DecisionLog_Guardrail.json'

/** 断言解析成功并取出 run，失败时直接让测试报错而不是返回 undefined。 */
function mustParse(raw: unknown) {
  const r = parseDecisionLog(raw)
  if (!r.ok) {
    throw new Error('本应解析成功，实际失败：\n' + r.errors.join('\n'))
  }
  return r
}

// ---------------------------------------------------------------------------
describe('真实样例', () => {
  it('全绿样例：三层、零警告、零拦截', () => {
    const { run, warnings } = mustParse(greenSample)

    expect(warnings).toEqual([])
    expect(run.schemaVersion).toBe(SCHEMA_VERSION)
    expect(run.floors).toHaveLength(3)
    expect(run.floors.every((f) => f.validation.valid)).toBe(true)
    expect(run.floors.every((f) => f.validation.violations.length === 0)).toBe(true)
    expect(run.floors.every((f) => !f.trace.degraded)).toBe(true)
  })

  it('全绿样例：数值只在护栏后出现（项目的核心主张）', () => {
    const { run } = mustParse(greenSample)
    const withRules = run.floors.filter((f) => f.rawIntent.ruleIntents.length > 0)
    expect(withRules.length).toBeGreaterThan(0)

    for (const f of withRules) {
      // 护栏前：只有 tag/level，类型上就装不下数值
      for (const intent of f.rawIntent.ruleIntents) {
        expect(Object.keys(intent).sort()).toEqual(['level', 'tag'])
      }
      // 护栏后：multiplier 在此首次出现
      for (const mod of f.decision.ruleModifiers) {
        expect(typeof mod.multiplier).toBe('number')
      }
    }
  })

  it('护栏样例：三道护栏各拦一层，且都降级了', () => {
    const { run, warnings } = mustParse(guardrailSample)

    expect(warnings).toEqual([])
    expect(run.floors).toHaveLength(4)

    const byFloor = Object.fromEntries(run.floors.map((f) => [f.floorIndex, f]))

    // F0 观察层：不经 Provider，全绿
    expect(byFloor[0].trace.providerId).toBe('ObserveFloor')
    expect(byFloor[0].validation.valid).toBe(true)

    // F1/F2/F3 各被一道不同的护栏拦下
    const expected = { 1: 'Conflict', 2: 'Budget', 3: 'Schema' } as const
    for (const [floorIndex, guard] of Object.entries(expected)) {
      const f = byFloor[Number(floorIndex)]
      expect(f.validation.valid).toBe(false)
      expect(f.validation.violations).toHaveLength(1)
      expect(f.validation.violations[0].guard).toBe(guard)
      expect(f.trace.degraded).toBe(true)
      expect(f.trace.degradeReason).not.toBe('')
    }
  })

  it('护栏样例：被拦的意图仍然完整保留（否则页面无从展示"想改什么"）', () => {
    const { run } = mustParse(guardrailSample)
    const f1 = run.floors.find((f) => f.floorIndex === 1)!

    // 日志存的是**护栏前**的原始 Intent，不是降级后的结果
    expect(f1.rawIntent.ruleIntents.map((r) => r.tag).sort()).toEqual([
      'Rule.Ammo',
      'Rule.RangedDamage',
    ])
    // 而最终生效的是本地 Provider 重新决策出来的，与被拦的那份不同
    expect(f1.decision.ruleModifiers.length).toBeGreaterThan(0)
  })
})

// ---------------------------------------------------------------------------
describe('护栏名归一化', () => {
  // UE 侧 UEnum::GetValueAsString() 带上了 C++ 类型名前缀。
  // 已导出的日志改不了，所以两种形式都必须吃得下。
  it('剥掉 ESHMGuardrail:: 前缀', () => {
    expect(normalizeGuard('ESHMGuardrail::Conflict')).toBe('Conflict')
    expect(normalizeGuard('Conflict')).toBe('Conflict')
    expect(normalizeGuard('')).toBe('')
  })

  it('归一化后仍保留原值，便于排查契约漂移', () => {
    const { run } = mustParse(guardrailSample)
    const v = run.floors.find((f) => f.floorIndex === 1)!.validation.violations[0]
    expect(v.guard).toBe('Conflict')
    // rawGuard 存的是文件里的原样。**故意不写死成某一种形式**：
    // 现有样例导出于 UE 侧修复之前（带 ESHMGuardrail:: 前缀），修复后重导会是裸名，
    // 两种都对——把任一种钉死，都会让另一种情况下这条测试无故变红。
    expect(['Conflict', 'ESHMGuardrail::Conflict']).toContain(v.rawGuard)
    expect(normalizeGuard(v.rawGuard)).toBe(v.guard)
  })

  it('未知护栏原样透传，不被吞掉', () => {
    const { run } = mustParse({
      schemaVersion: 1,
      floors: [
        {
          floorIndex: 0,
          validation: { valid: false, violations: [{ guard: 'ESHMGuardrail::Telemetry', detail: '第五道' }] },
        },
      ],
    })
    const v = run.floors[0].validation.violations[0]
    expect(v.guard).toBe('Telemetry')
    expect(v.detail).toBe('第五道')
  })
})

// ---------------------------------------------------------------------------
describe('该失败的必须失败', () => {
  it.each([
    ['数字', 42],
    ['字符串', 'hello'],
    ['null', null],
    ['数组', [1, 2, 3]],
  ])('顶层是%s → 报错', (_name, raw) => {
    const r = parseDecisionLog(raw)
    expect(r.ok).toBe(false)
  })

  it('缺 schemaVersion → 报错并指明期望版本', () => {
    const r = parseDecisionLog({ hello: 'world' })
    expect(r.ok).toBe(false)
    if (r.ok) return
    expect(r.errors.join()).toContain(String(SCHEMA_VERSION))
  })

  it('版本不匹配 → 拒绝解析，不静默渲染', () => {
    const r = parseDecisionLog({ schemaVersion: 99, floors: [{ floorIndex: 0 }] })
    expect(r.ok).toBe(false)
    if (r.ok) return
    // 版本不符时字段语义可能已变，显示错误结论比打不开更糟
    expect(r.errors.join()).toContain('99')
  })

  it('缺 floors / floors 为空 / floors 全不可用 → 报错', () => {
    expect(parseDecisionLog({ schemaVersion: 1 }).ok).toBe(false)
    expect(parseDecisionLog({ schemaVersion: 1, floors: [] }).ok).toBe(false)
    expect(parseDecisionLog({ schemaVersion: 1, floors: [1, 'x', null] }).ok).toBe(false)
  })

  it('一次收集全部问题，而不是抛第一个就停', () => {
    const r = parseDecisionLog({ schemaVersion: 1, floors: [1, 'x', null] })
    expect(r.ok).toBe(false)
    if (r.ok) return
    // 三层都不可用，应该三条警告都在
    expect(r.warnings).toHaveLength(3)
  })
})

// ---------------------------------------------------------------------------
describe('该降级的必须降级（不能崩）', () => {
  const partial = {
    schemaVersion: 1,
    floors: [
      {
        floorIndex: 7,
        profile: { confidence: 'oops', dominantArchetype: 'Archetype.未来原型' },
        context: { challengeBudget: 'NaN' },
        rawIntent: { challengeLevel: '未来等级', enemyWeights: { 'Enemy.Grunt': 'x' } },
        trace: { providerId: 'FutureProvider', degraded: true },
      },
    ],
  }

  it('字段类型不符 → 用默认值 + 警告，仍可渲染', () => {
    const { run, warnings } = mustParse(partial)
    expect(run.floors).toHaveLength(1)
    expect(warnings.length).toBeGreaterThan(0)

    const f = run.floors[0]
    expect(f.profile.confidence).toBe(0.5) // 中性默认
    expect(f.context.challengeBudget).toBe(0)
    expect(f.rawIntent.enemyWeights).toEqual({}) // 非数字权重被跳过
  })

  it('缺失的整段（decision/validation）→ 空结构而非崩溃', () => {
    const { run } = mustParse(partial)
    const f = run.floors[0]
    expect(f.decision.ruleModifiers).toEqual([])
    expect(f.validation.violations).toEqual([])
    expect(f.validation.valid).toBe(true) // 无违规时默认 valid
  })

  it('未知枚举原样透传，不回落成错误的已知值', () => {
    const { run } = mustParse(partial)
    const f = run.floors[0]
    expect(f.rawIntent.challengeLevel).toBe('未来等级')
    expect(f.trace.providerId).toBe('FutureProvider')
    expect(f.profile.dominantArchetype).toBe('Archetype.未来原型')
  })

  it('snapshot 缺失是正常的，不产生警告', () => {
    // 契约 .h 里声明了 Key_Snapshot，但实测导出没有这个字段。
    // 把它当异常会让每一份真实日志都刷一屏警告。
    const { warnings } = mustParse(greenSample)
    expect(warnings.join()).not.toContain('snapshot')
  })

  it('坏的层被跳过，好的层照常解析', () => {
    const { run } = mustParse({
      schemaVersion: 1,
      floors: [{ floorIndex: 0 }, 'garbage', { floorIndex: 1 }],
    })
    expect(run.floors.map((f) => f.floorIndex)).toEqual([0, 1])
  })

  it('层按 floorIndex 排序，乱序输入不会让时间轴撒谎', () => {
    const { run } = mustParse({
      schemaVersion: 1,
      floors: [{ floorIndex: 2 }, { floorIndex: 0 }, { floorIndex: 1 }],
    })
    expect(run.floors.map((f) => f.floorIndex)).toEqual([0, 1, 2])
  })

  it('totalFloors 缺失时用实际层数兜底', () => {
    const { run } = mustParse({
      schemaVersion: 1,
      floors: [{ floorIndex: 0 }, { floorIndex: 1 }],
    })
    expect(run.totalFloors).toBe(2)
  })
})
