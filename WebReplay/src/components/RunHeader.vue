<script setup lang="ts">
// 顶部概览：这一局是什么 + 一眼看清护栏干了多少活。
// 统计数字全部由 floors 现算（computeRunStats），UE 侧不必为此多导出任何字段。
import { computed } from 'vue'
import type { DecisionRun } from '../types/decisionLog'
import { GUARD_ORDER } from '../types/decisionLog'
import { GUARD_LABELS, labelOf } from '../types/labels'
import { computeRunStats, formatElapsed } from '../types/runStats'

const props = defineProps<{ run: DecisionRun }>()

const stats = computed(() => computeRunStats(props.run))

/** 分道明细只列真的拦到过的那几道，全零时整块不显示——一排 0 是噪音。 */
const hitGuards = computed(() =>
  GUARD_ORDER.filter((g) => stats.value.byGuard[g] > 0).map((g) => ({
    guard: g,
    label: labelOf(GUARD_LABELS, g),
    count: stats.value.byGuard[g],
  })),
)

const unknownGuards = computed(() => Object.entries(stats.value.unknownGuards))

const startedAtText = computed(() => {
  const raw = props.run.startedAt
  if (!raw) return '—'
  const d = new Date(raw)
  // 解析不了就原样显示。日志里的时间戳格式由 UE 的 ToIso8601() 决定，
  // 这里不该因为格式变化就显示 "Invalid Date"
  return Number.isNaN(d.getTime()) ? raw : d.toLocaleString()
})
</script>

<template>
  <header>
    <div class="titleRow">
      <h1>山海镜 · 决策回放器</h1>
      <p class="tagline">
        LLM 想改什么 → 四道护栏拦没拦 → 实际改了什么
      </p>
    </div>

    <dl class="meta">
      <div><dt>本局</dt><dd class="mono">{{ run.runId || '—' }}</dd></div>
      <div><dt>开始于</dt><dd>{{ startedAtText }}</dd></div>
      <div><dt>层数</dt><dd>{{ stats.totalFloors }}</dd></div>
    </dl>

    <ul class="stats">
      <li>
        <span class="num">{{ stats.llmDirect }}</span>
        <span class="lbl">层 LLM 直采</span>
      </li>
      <li :class="{ hot: stats.degraded > 0 }">
        <span class="num">{{ stats.degraded }}</span>
        <span class="lbl">层降级</span>
      </li>
      <li :class="{ hot: stats.violations > 0 }">
        <span class="num">{{ stats.violations }}</span>
        <span class="lbl">次护栏拦截</span>
      </li>
      <li>
        <span class="num">{{ formatElapsed(stats.avgElapsedMs) }}</span>
        <span class="lbl">平均耗时（{{ stats.timedFloors }} 层）</span>
      </li>
    </ul>

    <p v-if="hitGuards.length || unknownGuards.length" class="breakdown">
      <span class="dim">分道：</span>
      <span v-for="g in hitGuards" :key="g.guard" class="chip bad">
        {{ g.label }} <b>{{ g.count }}</b>
      </span>
      <span v-for="[name, n] in unknownGuards" :key="name" class="chip unknown">
        {{ name }} <b>{{ n }}</b>
      </span>
    </p>
  </header>
</template>

<style scoped>
header {
  border-bottom: 1px solid var(--border);
  padding-bottom: 1.25rem;
}

.titleRow {
  display: flex;
  align-items: baseline;
  flex-wrap: wrap;
  gap: 0.5rem 1rem;
}

h1 {
  font-size: 1.5rem;
  letter-spacing: -0.01em;
}

.tagline {
  margin: 0;
  color: var(--text-dim);
  font-size: 0.88rem;
}

.meta {
  display: flex;
  flex-wrap: wrap;
  gap: 0.4rem 1.75rem;
  margin: 0.85rem 0 0;
  font-size: 0.85rem;
}
.meta div {
  display: flex;
  gap: 0.45rem;
}
.meta dt {
  color: var(--text-dim);
}
.meta dd {
  margin: 0;
  color: var(--text-h);
}

.stats {
  list-style: none;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  padding: 0;
  margin: 1rem 0 0;
}
.stats li {
  display: flex;
  flex-direction: column;
  gap: 0.1rem;
  padding: 0.5rem 0.9rem;
  border: 1px solid var(--border);
  border-radius: 0.4rem;
  background: var(--bg-panel);
  min-width: 6.5rem;
}
/* 有降级/有拦截时才染色——全零的一局不该看起来像出了事 */
.stats li.hot {
  border-color: color-mix(in srgb, var(--bad) 45%, transparent);
  background: var(--bad-bg);
}
.stats .num {
  font-size: 1.35rem;
  font-weight: 600;
  color: var(--text-h);
  line-height: 1.15;
}
.stats li.hot .num {
  color: var(--bad);
}
.stats .lbl {
  font-size: 0.75rem;
  color: var(--text-dim);
}

.breakdown {
  margin: 0.75rem 0 0;
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 0.35rem;
  font-size: 0.85rem;
}
.chip.bad {
  color: var(--bad);
  background: var(--bad-bg);
  border-color: color-mix(in srgb, var(--bad) 35%, transparent);
}
/* 未知护栏用中性色：它可能是 UE 侧新加的第五道，不该被当成错误 */
.chip.unknown {
  color: var(--warn);
  background: var(--warn-bg);
  border-color: color-mix(in srgb, var(--warn) 35%, transparent);
}
</style>
