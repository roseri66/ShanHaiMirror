<script setup lang="ts">
// 敌人配比的前后对比。
//
// 两条 100% 堆叠条（护栏前 / 护栏后），同序对齐 —— 4 个类别、2 个状态，
// 是堆叠条唯一真正好读的场景。
//
// 分类色取自 dataviz 规范的 1–4 号槽位，浅深两套均通过验证器
// （相邻 CVD ΔE 9.1 浅 / 8.4 深）。浅色模式下 aqua 与 yellow 对白底低于 3:1，
// 规范判为 WARN 且**不可忽略**，补偿手段是下方那行带数值的图例——
// 因此那一行不是装饰，是可达性要求的一部分：identity 永远不能只靠颜色。
import { computed } from 'vue'
import type { Floor } from '../types/decisionLog'
import { weightComparison } from '../types/floorView'
import { ENEMY_LABELS, labelOf } from '../types/labels'
import { formatPercent } from '../types/runStats'

const props = defineProps<{ floor: Floor }>()

const cmp = computed(() => weightComparison(props.floor))

/** 固定槽位顺序，**不按数值排序** —— 颜色跟随实体，不跟随排名。 */
const ORDER = ['Enemy.Grunt', 'Enemy.Tank', 'Enemy.Rush', 'Enemy.Shooter']

const rows = computed(() => {
  const known = cmp.value.rows.filter((r) => ORDER.includes(r.tag))
  const rest = cmp.value.rows.filter((r) => !ORDER.includes(r.tag))
  known.sort((a, b) => ORDER.indexOf(a.tag) - ORDER.indexOf(b.tag))
  return [...known, ...rest]
})

function slotOf(tag: string): number {
  const i = ORDER.indexOf(tag)
  // 未知原型统一落到最后一个槽位，不生成新色（分类色不循环、不生成）
  return i >= 0 ? i + 1 : ORDER.length + 1
}

/** 权重为 0 的段不渲染，否则 2px 间隙会堆出一串看不出内容的缝 */
function visible(list: Array<{ tag: string; value: number }>) {
  return list.filter((s) => s.value > 0.0001)
}

const beforeSegs = computed(() =>
  visible(rows.value.map((r) => ({ tag: r.tag, value: r.before ?? 0 }))),
)
const afterSegs = computed(() =>
  visible(rows.value.map((r) => ({ tag: r.tag, value: r.after }))),
)

const changedTags = computed(() => new Set(rows.value.filter((r) => r.changed).map((r) => r.tag)))
</script>

<template>
  <div class="weights">
    <template v-if="cmp.comparable">
      <div class="row">
        <span class="cap">护栏前</span>
        <div class="bar">
          <span
            v-for="s in beforeSegs"
            :key="s.tag"
            class="seg"
            :data-slot="slotOf(s.tag)"
            :style="{ flexGrow: s.value }"
            :title="`${labelOf(ENEMY_LABELS, s.tag)} ${formatPercent(s.value)}`"
          />
        </div>
      </div>
      <div class="row">
        <span class="cap">护栏后</span>
        <div class="bar">
          <span
            v-for="s in afterSegs"
            :key="s.tag"
            class="seg"
            :class="{ changed: changedTags.has(s.tag) }"
            :data-slot="slotOf(s.tag)"
            :style="{ flexGrow: s.value }"
            :title="`${labelOf(ENEMY_LABELS, s.tag)} ${formatPercent(s.value)}`"
          />
        </div>
      </div>
    </template>

    <div v-else class="row">
      <span class="cap">配比</span>
      <div class="bar">
        <span
          v-for="s in afterSegs"
          :key="s.tag"
          class="seg"
          :data-slot="slotOf(s.tag)"
          :style="{ flexGrow: s.value }"
          :title="`${labelOf(ENEMY_LABELS, s.tag)} ${formatPercent(s.value)}`"
        />
      </div>
    </div>

    <!--
      图例 + 数值。**不是装饰**：浅色模式下两个槽位对白底低于 3:1，
      dataviz 规范要求以可见标签或表格视图作为补偿。同时它也保证
      "谁是谁" 永远不只靠颜色分辨。
    -->
    <ul class="legend">
      <li v-for="r in rows" :key="r.tag" :class="{ changed: r.changed }">
        <span class="swatch" :data-slot="slotOf(r.tag)" aria-hidden="true" />
        <span class="who">{{ labelOf(ENEMY_LABELS, r.tag) }}</span>
        <span v-if="cmp.comparable" class="val">
          {{ formatPercent(r.before ?? 0) }}
          <span class="arrow" :class="{ on: r.changed }">→</span>
          <b>{{ formatPercent(r.after) }}</b>
        </span>
        <span v-else class="val"><b>{{ formatPercent(r.after) }}</b></span>
      </li>
    </ul>

    <p v-if="!cmp.comparable" class="note dim">
      这一层没有调用 Provider（观察层），因此没有「护栏前」的配比可对照——
      显示的是本层实际生效的配比。
    </p>
  </div>
</template>

<style scoped>
.weights {
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
}

.row {
  display: flex;
  align-items: center;
  gap: 0.55rem;
}
.cap {
  flex: 0 0 3.2rem;
  font-size: 0.72rem;
  color: var(--text-dim);
  text-align: right;
}

.bar {
  flex: 1 1 auto;
  display: flex;
  /* 段与段之间留 2px 表面色缝隙（dataviz 标记规范） */
  gap: 2px;
  height: 1.1rem;
  min-width: 0;
}
.seg {
  min-width: 3px;
  background: var(--slot-1);
}
/* 数据端 4px 圆角，只在两头 */
.seg:first-child {
  border-radius: 4px 0 0 4px;
}
.seg:last-child {
  border-radius: 0 4px 4px 0;
}
.seg:only-child {
  border-radius: 4px;
}
/* 变动项：2px 表面色描边把它从相邻段里挑出来 */
.seg.changed {
  outline: 2px solid var(--bg-panel);
  outline-offset: -2px;
  box-shadow: 0 0 0 1px var(--text-h);
}

.seg[data-slot='1'],
.swatch[data-slot='1'] { background: var(--slot-1); }
.seg[data-slot='2'],
.swatch[data-slot='2'] { background: var(--slot-2); }
.seg[data-slot='3'],
.swatch[data-slot='3'] { background: var(--slot-3); }
.seg[data-slot='4'],
.swatch[data-slot='4'] { background: var(--slot-4); }
.seg[data-slot='5'],
.swatch[data-slot='5'] { background: var(--text-dim); }

.legend {
  list-style: none;
  margin: 0.15rem 0 0;
  padding: 0 0 0 3.75rem;
  display: flex;
  flex-wrap: wrap;
  gap: 0.2rem 0.9rem;
  font-size: 0.75rem;
}
.legend li {
  display: flex;
  align-items: center;
  gap: 0.3rem;
}
.swatch {
  width: 0.6rem;
  height: 0.6rem;
  border-radius: 2px;
  flex: 0 0 auto;
}
.who {
  color: var(--text);
}
/* 数值用文字色，不用系列色——色块在旁边负责标识 */
.val {
  color: var(--text-dim);
}
.val b {
  color: var(--text-h);
  font-weight: 600;
}
.arrow {
  opacity: 0.5;
}
.arrow.on {
  opacity: 1;
  color: var(--bad);
  font-weight: 700;
}
.legend li.changed .who {
  color: var(--text-h);
  font-weight: 600;
}

.note {
  margin: 0.2rem 0 0;
  padding-left: 3.75rem;
  font-size: 0.72rem;
}

@media (max-width: 34rem) {
  .cap {
    flex-basis: 2.6rem;
  }
  .legend,
  .note {
    padding-left: 0;
  }
}
</style>
