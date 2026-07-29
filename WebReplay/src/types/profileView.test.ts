// ============================================================================
// 画像视图模型的测试（M3：雷达图 + 约束）
//
// 雷达图的几何是纯数学，最该被测：起始角差 90° 或方向搞反，图看着"也挺像那么回事"，
// 但每根轴对应的维度全错了 —— 这种错误肉眼极难发现，只有测试能钉住。
// ============================================================================

import { describe, expect, it } from 'vitest'
import {
  PLACEHOLDER_DIMENSION,
  hasObservations,
  profileAxes,
  radarPoints,
} from './profileView'
import { parseDecisionLog } from './parseDecisionLog'
import type { Floor } from './decisionLog'
import greenSample from '../../../Docs/samples/DecisionLog_Sample.json'
import guardrailRun from '../../../Docs/samples/DecisionLog_Guardrail_Run.json'

function floorOf(raw: unknown, index: number): Floor {
  const r = parseDecisionLog(raw)
  if (!r.ok) throw new Error(r.errors.join('\n'))
  return r.run.floors.find((x) => x.floorIndex === index)!
}

// ---------------------------------------------------------------------------
describe('profileAxes · 只画有数据源的维度', () => {
  it('四根轴，顺序与游戏内报告卡一致', () => {
    // SHMDirectorHUD.cpp:159-162 的排列，网页与游戏内必须叫同样的名字、同样的顺序
    const axes = profileAxes(floorOf(greenSample, 1).profile)
    expect(axes.map((a) => a.key)).toEqual([
      'buildConcentration',
      'combatEfficiency',
      'survivalPressure',
      'strategySwitch',
    ])
    expect(axes.map((a) => a.label)).toEqual(['打法集中度', '战斗效率', '生存压力', '策略切换'])
  })

  it('resourceSurplus 不进雷达图', () => {
    // 它是恒为 50 的中性占位：D-09 砍了道具系统 → 无数据源 → ProfileAnalyzer 返回中性值。
    // 画进去等于把一个常量伪装成一条观测结论。游戏内报告卡同样没有显示它。
    const axes = profileAxes(floorOf(greenSample, 1).profile)
    expect(axes.map((a) => a.key)).not.toContain('resourceSurplus')
    expect(PLACEHOLDER_DIMENSION.key).toBe('resourceSurplus')
    expect(PLACEHOLDER_DIMENSION.reason).toContain('D-09')
  })

  it('三份样例里 resourceSurplus 确实恒为 50（占位判断的依据）', () => {
    // 这条测试是"占位"这个结论本身的依据。哪天 UE 侧真接了道具系统、
    // 这个值开始变化，它会变红 —— 提醒把这一维放回雷达图。
    for (const floorIndex of [1, 2]) {
      expect(floorOf(greenSample, floorIndex).profile.resourceSurplus).toBe(50)
      expect(floorOf(guardrailRun, floorIndex).profile.resourceSurplus).toBe(50)
    }
  })

  it('取值原样传出，不做归一化', () => {
    const axes = profileAxes(floorOf(guardrailRun, 1).profile)
    const eff = axes.find((a) => a.key === 'combatEfficiency')!
    expect(eff.value).toBeCloseTo(91.7, 0)
  })
})

// ---------------------------------------------------------------------------
describe('hasObservations · 区分"没观测到"和"观测结果是零"', () => {
  it('真实对局 F0：四维全零且原型未定 → 尚无观测', () => {
    // 首层是观察层，行为记录器还没攒到任何东西。
    // 画成一个塌到圆心的四边形，看着像图坏了，其实是没数据。
    expect(hasObservations(floorOf(guardrailRun, 0).profile)).toBe(false)
  })

  it('有任一维非零 → 有观测', () => {
    expect(hasObservations(floorOf(guardrailRun, 1).profile)).toBe(true)
    expect(hasObservations(floorOf(greenSample, 2).profile)).toBe(true)
  })

  it('全零但原型已定 → 仍算有观测（不能只看数值）', () => {
    expect(
      hasObservations({
        buildConcentration: 0,
        combatEfficiency: 0,
        resourceSurplus: 0,
        strategySwitch: 0,
        survivalPressure: 0,
        confidence: 0.5,
        dominantArchetype: 'Archetype.Survivor',
      }),
    ).toBe(true)
  })
})

// ---------------------------------------------------------------------------
describe('radarPoints · 几何', () => {
  const R = 100

  it('第一根轴指向正上方', () => {
    const pts = radarPoints([100, 0, 0, 0], R)
    expect(pts[0].x).toBeCloseTo(0, 6)
    expect(pts[0].y).toBeCloseTo(-R, 6) // SVG 的 y 向下，所以正上方是负值
  })

  it('四根轴按顺时针分布：上 → 右 → 下 → 左', () => {
    const pts = radarPoints([100, 100, 100, 100], R)
    expect(pts[1].x).toBeCloseTo(R, 6)
    expect(pts[1].y).toBeCloseTo(0, 6)
    expect(pts[2].x).toBeCloseTo(0, 6)
    expect(pts[2].y).toBeCloseTo(R, 6)
    expect(pts[3].x).toBeCloseTo(-R, 6)
    expect(pts[3].y).toBeCloseTo(0, 6)
  })

  it('数值按 0–100 线性缩放到半径', () => {
    const pts = radarPoints([50, 0, 0, 0], R)
    expect(pts[0].y).toBeCloseTo(-R / 2, 6)

    const zero = radarPoints([0, 0, 0, 0], R)
    expect(zero.every((p) => Math.abs(p.x) < 1e-9 && Math.abs(p.y) < 1e-9)).toBe(true)
  })

  it('越界值被夹到 0–100，不画到图外', () => {
    // 画像理论上是 0-100，但日志可能来自旧版本或被手改过。
    // 夹住比画出一个戳穿边框的尖角好 —— 前端对 JSON 同样不该信任。
    const pts = radarPoints([200, -50, 0, 0], R)
    expect(pts[0].y).toBeCloseTo(-R, 6)
    expect(Math.hypot(pts[1].x, pts[1].y)).toBeCloseTo(0, 6)
  })

  it('点数与输入维数一致，支持任意维数', () => {
    expect(radarPoints([1, 2, 3], R)).toHaveLength(3)
    expect(radarPoints([1, 2, 3, 4, 5], R)).toHaveLength(5)
  })
})
