// ============================================================================
// 画像视图模型 —— 第 Ⅰ 列「我看到的」
//
// 两个决定值得写下来：
//
// ① **`resourceSurplus` 不进雷达图。** 它在每一份真实日志里都恰好是 50 ——
//    D-09 砍掉道具系统后这一维没有数据源，`ProfileAnalyzer` 返回中性值。
//    把一个常量画成五边形的一个角，读者会当成"这个玩家资源中等"这条观测结论。
//    游戏内的报告卡（SHMDirectorHUD.cpp:159-162）同样没有显示它 ——
//    网页跟着游戏走，并在图下方显式说明这一维为什么缺席。
//
// ② **`confidence` 也不进雷达图。** 量纲不同（0–1 vs 0–100），
//    塞进同一张图会让图撒谎。它单独用一条进度条表示。
// ============================================================================

import type { Profile } from './decisionLog'

export interface ProfileAxis {
  key: string
  /** 与游戏内报告卡逐字一致（SHMDirectorHUD.cpp），网页不另起名字 */
  label: string
  /** 0–100，原样传出不做归一化 */
  value: number
}

/**
 * 雷达图的四根轴，顺序与游戏内报告卡一致。
 * 顺序是固定的：轴的位置代表维度身份，不能按数值排序。
 */
export function profileAxes(profile: Profile): ProfileAxis[] {
  return [
    { key: 'buildConcentration', label: '打法集中度', value: profile.buildConcentration },
    { key: 'combatEfficiency', label: '战斗效率', value: profile.combatEfficiency },
    { key: 'survivalPressure', label: '生存压力', value: profile.survivalPressure },
    { key: 'strategySwitch', label: '策略切换', value: profile.strategySwitch },
  ]
}

/**
 * 契约里有、但没有数据源的那一维。
 * 单独列出来而不是悄悄丢掉 —— 契约有五维是事实，只画四维也是事实，两者都要说。
 */
export const PLACEHOLDER_DIMENSION = {
  key: 'resourceSurplus',
  label: '资源盈余',
  reason:
    '中性占位：道具系统已砍（DECISIONS D-09），这一维没有数据源，' +
    'ProfileAnalyzer 恒返回 50，本地 Provider 也明确不消费它。' +
    '故画像实际是四维 + 一个占位。游戏内报告卡同样不显示它。',
} as const

/**
 * 这一层是否已经观测到东西。
 *
 * 首层是观察层，行为记录器还没攒到任何数据，四维全零。
 * 画出来是一个塌到圆心的四边形，看着像图坏了 —— 其实是"还没看到"。
 * **「没观测到」和「观测结果是零」必须能区分**，这和 UE 侧
 * 「比率型维度无分母时返回中性 50、计数型无事件返回 0」是同一条纪律。
 *
 * 只看数值不够：原型一旦判定出来，就说明确实观测过了。
 */
export function hasObservations(profile: Profile): boolean {
  const anyValue = profileAxes(profile).some((a) => a.value > 0)
  const hasArchetype = Boolean(profile.dominantArchetype) && profile.dominantArchetype !== 'None'
  return anyValue || hasArchetype
}

// --- 雷达图几何 -------------------------------------------------------------

export interface Point {
  x: number
  y: number
}

/**
 * 把一组 0–100 的值算成雷达图顶点，圆心为 (0, 0)。
 *
 * 第一根轴指向正上方，其余顺时针均分。SVG 的 y 轴向下，所以正上方是负 y。
 * 手写这几行的理由见设计文档：需求只是一个静态多边形，
 * 为它引一个通用图表库（~300KB）不划算。
 */
export function radarPoints(values: number[], radius: number): Point[] {
  const n = values.length
  return values.map((raw, i) => {
    // 夹到 0–100：日志可能来自旧版本或被手改过，画出一个戳穿边框的尖角
    // 比夹住更糟 —— 前端对 JSON 同样不该信任
    const v = Math.min(100, Math.max(0, Number.isFinite(raw) ? raw : 0))
    const r = (radius * v) / 100
    const angle = -Math.PI / 2 + (i * 2 * Math.PI) / n
    return { x: r * Math.cos(angle), y: r * Math.sin(angle) }
  })
}

/** 顶点数组 → SVG `points` 属性 */
export function toPolygonPoints(points: Point[]): string {
  return points.map((p) => `${p.x.toFixed(2)},${p.y.toFixed(2)}`).join(' ')
}
