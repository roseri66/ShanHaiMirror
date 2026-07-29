<script setup lang="ts">
// 第 Ⅰ 列「我看到的」—— 手写 SVG 雷达图。
//
// 不引 ECharts：需求只是一个静态四边形，为它引 ~300KB 通用图表库不划算。
// 判据是「专用件的复杂度 vs 需求的复杂度」——同一条判据在别的场景会得出相反结论
// （连接池就该用 HikariCP 而不是自研）。
//
// 单系列，所以不需要图例（标题已经指明它是什么），四根轴各自直接标注。
import { computed } from 'vue'
import type { Profile } from '../types/decisionLog'
import {
  PLACEHOLDER_DIMENSION,
  hasObservations,
  profileAxes,
  radarPoints,
  toPolygonPoints,
} from '../types/profileView'
import { ARCHETYPE_LABELS, labelOf } from '../types/labels'

const props = defineProps<{ profile: Profile }>()

const R = 62 // 数据区半径；viewBox 留出标签空间
const RINGS = [25, 50, 75, 100]

const axes = computed(() => profileAxes(props.profile))
const observed = computed(() => hasObservations(props.profile))

/** 每根轴的端点（满值），用来画轴线与摆标签 */
const spokes = computed(() => radarPoints(axes.value.map(() => 100), R))
const shape = computed(() => toPolygonPoints(radarPoints(axes.value.map((a) => a.value), R)))

function ringPoints(pct: number): string {
  return toPolygonPoints(radarPoints(axes.value.map(() => pct), R))
}

/** 标签摆在轴端外侧一点，按方向调整锚点，避免压住图形 */
function labelPos(i: number) {
  const p = radarPoints(
    axes.value.map(() => 100),
    R + 14,
  )[i]
  const anchor = Math.abs(p.x) < 1 ? 'middle' : p.x > 0 ? 'start' : 'end'
  return { x: p.x, y: p.y + (Math.abs(p.x) < 1 ? (p.y < 0 ? -2 : 8) : 4), anchor }
}

const confidencePct = computed(() => Math.round(props.profile.confidence * 100))
</script>

<template>
  <div class="radar">
    <div class="chartWrap">
      <svg viewBox="-100 -92 200 184" role="img" :aria-label="`画像雷达图：${axes.map((a) => `${a.label} ${a.value.toFixed(0)}`).join('，')}`">
        <g>
          <!-- 网格：recessive，不与数据争视觉 -->
          <polygon v-for="r in RINGS" :key="r" :points="ringPoints(r)" class="ring" />
          <line v-for="(s, i) in spokes" :key="i" x1="0" y1="0" :x2="s.x" :y2="s.y" class="spoke" />

          <polygon v-if="observed" :points="shape" class="shape" />
          <circle v-else r="2.5" class="empty" />

          <text
            v-for="(a, i) in axes"
            :key="a.key"
            :x="labelPos(i).x"
            :y="labelPos(i).y"
            :text-anchor="labelPos(i).anchor"
            class="axisLabel"
          >
            {{ a.label }}
          </text>
        </g>
      </svg>
      <p v-if="!observed" class="noObs dim">首层为观察层，尚未观测到任何行为</p>
    </div>

    <!-- 数值直接列出：既是可达性要求（不靠图形读数），也方便和游戏内报告卡对账 -->
    <ul class="values">
      <li v-for="a in axes" :key="a.key">
        <span class="k">{{ a.label }}</span>
        <span class="v">{{ a.value.toFixed(0) }}</span>
      </li>
    </ul>

    <!-- confidence 量纲不同（0–1），单独一条，绝不塞进雷达图 -->
    <div class="confidence">
      <div class="cHead">
        <span class="k">判断置信度</span>
        <span class="v">{{ profile.confidence.toFixed(2) }}</span>
      </div>
      <div class="track"><div class="fill" :style="{ width: confidencePct + '%' }" /></div>
      <p class="hint dim">连续多层同打法才升高；低于 0.60 时护栏禁止重度规则</p>
    </div>

    <p class="archetype">
      主导原型
      <b>{{ labelOf(ARCHETYPE_LABELS, profile.dominantArchetype) }}</b>
    </p>

    <!-- 契约有五维、这里只画四维，两件事都得说 -->
    <p class="placeholder dim">
      <b>{{ PLACEHOLDER_DIMENSION.label }}</b> 未列入：{{ PLACEHOLDER_DIMENSION.reason }}
    </p>
  </div>
</template>

<style scoped>
.radar {
  display: flex;
  flex-direction: column;
  gap: 0.6rem;
}

.chartWrap {
  position: relative;
}
svg {
  width: 100%;
  max-width: 15rem;
  height: auto;
  display: block;
  margin: 0 auto;
  overflow: visible;
}

.ring {
  fill: none;
  stroke: var(--border);
  stroke-width: 1;
}
.spoke {
  stroke: var(--border);
  stroke-width: 1;
}
.shape {
  fill: color-mix(in srgb, var(--slot-1) 28%, transparent);
  stroke: var(--slot-1);
  stroke-width: 2;
  stroke-linejoin: round;
}
.empty {
  fill: var(--text-dim);
}
/* 轴标签用文字色，不用系列色 */
.axisLabel {
  font-size: 9px;
  fill: var(--text-dim);
  font-family: var(--sans);
}

.noObs {
  margin: 0.2rem 0 0;
  text-align: center;
  font-size: 0.74rem;
}

.values {
  list-style: none;
  margin: 0;
  padding: 0;
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 0.15rem 0.8rem;
  font-size: 0.76rem;
}
.values li {
  display: flex;
  justify-content: space-between;
  gap: 0.5rem;
}
.k {
  color: var(--text-dim);
}
.v {
  color: var(--text-h);
  font-family: var(--mono);
  font-weight: 600;
}

.confidence {
  margin-top: 0.2rem;
}
.cHead {
  display: flex;
  justify-content: space-between;
  font-size: 0.76rem;
  margin-bottom: 0.2rem;
}
.track {
  height: 0.4rem;
  border-radius: 4px;
  background: var(--bg-sunken);
  border: 1px solid var(--border);
  overflow: hidden;
}
.fill {
  height: 100%;
  background: var(--slot-1);
  border-radius: 3px;
}
.hint {
  margin: 0.2rem 0 0;
  font-size: 0.7rem;
  line-height: 1.45;
}

.archetype {
  margin: 0.2rem 0 0;
  font-size: 0.8rem;
  color: var(--text-dim);
}
.archetype b {
  color: var(--text-h);
  margin-left: 0.3rem;
}

.placeholder {
  margin: 0.15rem 0 0;
  font-size: 0.68rem;
  line-height: 1.5;
  padding-top: 0.4rem;
  border-top: 1px dashed var(--border);
}
</style>
