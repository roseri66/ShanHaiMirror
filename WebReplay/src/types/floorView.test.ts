// ============================================================================
// 单层视图模型的测试（M2：护栏前后对照）
//
// M2 是整个页面的卖点，而它全部的说服力都建立在"对照算得对"上：
// 哪几盏灯该红、哪条规则被换掉了、哪个权重变了。算错一处，整屏结论就是错的。
// 所以这三个函数是纯函数并且先写测试。
// ============================================================================

import { describe, expect, it } from 'vitest'
import { guardBand, ruleComparison, weightComparison } from './floorView'
import { parseDecisionLog } from './parseDecisionLog'
import type { Floor } from './decisionLog'
import greenSample from '../../../Docs/samples/DecisionLog_Sample.json'
import guardrailRun from '../../../Docs/samples/DecisionLog_Guardrail_Run.json'
import guardrailIdeal from '../../../Docs/samples/DecisionLog_Guardrail_Ideal.json'

function floorOf(raw: unknown, index: number): Floor {
  const r = parseDecisionLog(raw)
  if (!r.ok) throw new Error(r.errors.join('\n'))
  const f = r.run.floors.find((x) => x.floorIndex === index)
  if (!f) throw new Error(`没有第 ${index} 层`)
  return f
}

// ---------------------------------------------------------------------------
describe('guardBand · 四道灯带', () => {
  it('恒定返回四盏灯，顺序与 Validate() 的调用顺序一致', () => {
    const band = guardBand(floorOf(greenSample, 1))
    expect(band.lamps.map((l) => l.guard)).toEqual(['Schema', 'Budget', 'Conflict', 'Fairness'])
  })

  it('全绿层：四盏全灭，无违规', () => {
    const band = guardBand(floorOf(greenSample, 1))
    expect(band.lamps.every((l) => !l.hit)).toBe(true)
    expect(band.lamps.every((l) => l.violations.length === 0)).toBe(true)
  })

  it('真实拦截层：只有规则互斥那盏亮，其余三盏灭', () => {
    const band = guardBand(floorOf(guardrailRun, 1))
    const hit = band.lamps.filter((l) => l.hit)
    expect(hit).toHaveLength(1)
    expect(hit[0].guard).toBe('Conflict')
    // detail 要能展开，所以必须挂在对应的灯上而不是丢掉
    expect(hit[0].violations[0].detail).toContain('互斥')
  })

  it('三份样例的三道拦截各自点亮正确的灯', () => {
    const expected: Array<[number, string]> = [
      [1, 'Conflict'],
      [2, 'Budget'],
      [3, 'Schema'],
    ]
    for (const [floorIndex, guard] of expected) {
      const band = guardBand(floorOf(guardrailIdeal, floorIndex))
      expect(band.lamps.filter((l) => l.hit).map((l) => l.guard)).toEqual([guard])
    }
  })

  it('同一道被拦多次时，违规全部挂在那盏灯上', () => {
    const band = guardBand({
      validation: {
        valid: false,
        violations: [
          { guard: 'Schema', rawGuard: 'Schema', detail: 'a' },
          { guard: 'Schema', rawGuard: 'Schema', detail: 'b' },
        ],
      },
    } as Floor)
    const schema = band.lamps.find((l) => l.guard === 'Schema')!
    expect(schema.violations.map((v) => v.detail)).toEqual(['a', 'b'])
  })

  it('未知护栏单独收集，不被四盏灯吞掉', () => {
    // UE 侧加第五道而前端没跟上时，这条违规必须还能显示出来
    const band = guardBand({
      validation: {
        valid: false,
        violations: [{ guard: 'Telemetry', rawGuard: 'ESHMGuardrail::Telemetry', detail: '第五道' }],
      },
    } as Floor)
    expect(band.lamps.every((l) => !l.hit)).toBe(true)
    expect(band.unknown).toHaveLength(1)
    expect(band.unknown[0].guard).toBe('Telemetry')
  })
})

// ---------------------------------------------------------------------------
describe('ruleComparison · 护栏前后的规则对照', () => {
  it('全绿层：规则原样通过，标记为保留', () => {
    const cmp = ruleComparison(floorOf(greenSample, 1))

    expect(cmp.identical).toBe(true)
    expect(cmp.before).toHaveLength(1)
    expect(cmp.before[0].status).toBe('kept')
    expect(cmp.after).toHaveLength(1)
    expect(cmp.after[0].status).toBe('fromIntent')
    // 核心主张：左侧无数值，右侧才有
    expect(cmp.after[0].mod.multiplier).toBeCloseTo(0.8, 5)
  })

  it('被拦层：左侧整套被丢弃，右侧整套是降级后新产生的', () => {
    // 真实数据：左 Ammo/light + RangedDamage/light，右 MeleeDamage/light ×0.90
    const cmp = ruleComparison(floorOf(guardrailRun, 1))

    expect(cmp.identical).toBe(false)
    expect(cmp.before.map((b) => b.status)).toEqual(['dropped', 'dropped'])
    expect(cmp.after.map((a) => a.status)).toEqual(['added'])
    expect(cmp.after[0].mod.tag).toBe('Rule.MeleeDamage')
  })

  it('配对看 tag+level 两项，同标签不同等级算换掉了', () => {
    // 理想夹具 F2：左 Ammo/medium…，右 Ammo/medium + Cooldown/medium
    // Ammo/medium 两边都有 → kept；其余各自 dropped / added
    const cmp = ruleComparison(floorOf(guardrailIdeal, 2))
    const kept = cmp.before.filter((b) => b.status === 'kept').map((b) => b.rule.tag)
    expect(kept).toContain('Rule.Ammo')

    // 同 tag 不同 level 不算保留
    const cmp2 = ruleComparison({
      rawIntent: { ruleIntents: [{ tag: 'Rule.Ammo', level: 'light' }] },
      decision: { ruleModifiers: [{ tag: 'Rule.Ammo', level: 'medium', multiplier: 0.7, cost: 20 }] },
    } as Floor)
    expect(cmp2.before[0].status).toBe('dropped')
    expect(cmp2.after[0].status).toBe('added')
    expect(cmp2.identical).toBe(false)
  })

  it('两侧都空时算一致（观察层没提出调整，也没产生调整）', () => {
    const cmp = ruleComparison(floorOf(guardrailRun, 0))
    expect(cmp.before).toEqual([])
    expect(cmp.after).toEqual([])
    expect(cmp.identical).toBe(true)
  })
})

// ---------------------------------------------------------------------------
describe('weightComparison · 敌人配比前后对照', () => {
  it('全绿层：配比原样通过，无变动项', () => {
    const cmp = weightComparison(floorOf(greenSample, 1))

    expect(cmp.comparable).toBe(true)
    expect(cmp.rows.every((r) => !r.changed)).toBe(true)
    expect(cmp.rows.map((r) => r.tag)).toContain('Enemy.Tank')
  })

  it('被拦层：配比被整体替换，四项全部标为变动', () => {
    // 真实数据：0.45/0.25/0.20/0.10 → 0.55/0.15/0.15/0.15
    const cmp = weightComparison(floorOf(guardrailRun, 1))

    expect(cmp.comparable).toBe(true)
    expect(cmp.rows).toHaveLength(4)
    expect(cmp.rows.every((r) => r.changed)).toBe(true)

    const grunt = cmp.rows.find((r) => r.tag === 'Enemy.Grunt')!
    expect(grunt.before).toBeCloseTo(0.45, 3)
    expect(grunt.after).toBeCloseTo(0.55, 3)
  })

  it('观察层 rawIntent 无配比 → comparable=false，不能渲染成"全都改了"', () => {
    // F0 压根没走 Provider，rawIntent.enemyWeights 是 {}。
    // 若照常做差集，会显示成"导演把四项权重从 0 改到 0.55"——纯属胡说。
    const cmp = weightComparison(floorOf(guardrailRun, 0))

    expect(cmp.comparable).toBe(false)
    expect(cmp.rows.every((r) => r.before === undefined)).toBe(true)
    expect(cmp.rows.every((r) => !r.changed)).toBe(true)
    // 但最终配比仍要能显示出来
    expect(cmp.rows).toHaveLength(4)
  })

  it('浮点尾巴不算变动（float32 落进 JSON 会带一串小数）', () => {
    const cmp = weightComparison({
      rawIntent: { enemyWeights: { 'Enemy.Grunt': 0.550000011920929 } },
      decision: { enemyWeights: { 'Enemy.Grunt': 0.55 } },
    } as unknown as Floor)
    expect(cmp.rows[0].changed).toBe(false)
  })

  it('只在一侧出现的原型也要列出来', () => {
    const cmp = weightComparison({
      rawIntent: { enemyWeights: { 'Enemy.Grunt': 0.5, 'Enemy.Tank': 0.5 } },
      decision: { enemyWeights: { 'Enemy.Grunt': 1 } },
    } as unknown as Floor)

    const tank = cmp.rows.find((r) => r.tag === 'Enemy.Tank')!
    expect(tank.before).toBeCloseTo(0.5, 5)
    expect(tank.after).toBe(0)
    expect(tank.changed).toBe(true)
  })
})
