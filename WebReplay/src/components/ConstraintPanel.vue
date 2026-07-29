<script setup lang="ts">
// 第 Ⅱ 列「约束」—— 链路第 ③ 步 CONSTRAIN 的产物。
//
// 这一列常被当成配角，其实它是"不信任 LLM 是设计前提"最直接的证据：
// **候选集在第 ③ 步就已经收敛**，Provider 只能在这几条里选，
// 想选 heavy、想选已连用满两层的规则，都够不着。
// 护栏是第二道保险，不是唯一一道。
import { computed } from 'vue'
import type { Floor } from '../types/decisionLog'
import { RULE_LEVEL_LABELS, RULE_TAG_LABELS, labelOf } from '../types/labels'

const props = defineProps<{ floor: Floor }>()

/** 按标签分组，同一条规则的不同强度并排 —— 候选集的结构本来就是这样 */
const grouped = computed(() => {
  const map = new Map<string, Array<{ level: string; cost: number }>>()
  for (const r of props.floor.context.availableRules) {
    const list = map.get(r.tag)
    if (list) list.push({ level: r.level, cost: r.cost })
    else map.set(r.tag, [{ level: r.level, cost: r.cost }])
  }
  return [...map.entries()].map(([tag, levels]) => ({ tag, levels }))
})

/** 本层实际用掉的预算，由生效的规则现算 */
const spent = computed(() =>
  props.floor.decision.ruleModifiers.reduce((sum, m) => sum + m.cost, 0),
)

const budget = computed(() => props.floor.context.challengeBudget)
const spentPct = computed(() =>
  budget.value > 0 ? Math.min(100, (spent.value / budget.value) * 100) : 0,
)
</script>

<template>
  <div class="constraints">
    <div class="budget">
      <span class="label">本层挑战预算</span>
      <div class="figure">
        <b>{{ budget }}</b>
        <span v-if="budget > 0" class="spent">已用 {{ spent }}</span>
      </div>
      <div v-if="budget > 0" class="track">
        <div class="fill" :style="{ width: spentPct + '%' }" />
      </div>
      <p v-else class="hint dim">首层预算为 0 —— 一条规则都买不起，"只观察"由预算自然实现</p>
    </div>

    <div class="rules">
      <span class="label">
        候选规则
        <span class="count">{{ floor.context.availableRules.length }} 条</span>
      </span>
      <p v-if="!grouped.length" class="hint dim">无候选</p>
      <ul v-else>
        <li v-for="g in grouped" :key="g.tag">
          <span class="tag">{{ labelOf(RULE_TAG_LABELS, g.tag) }}</span>
          <span v-for="l in g.levels" :key="l.level" class="chip">
            {{ labelOf(RULE_LEVEL_LABELS, l.level) }}
            <em>{{ l.cost }}</em>
          </span>
        </li>
      </ul>
      <p class="hint dim">
        候选集在链路第 ③ 步就已收敛 —— heavy 规则、已连用满两层的规则
        <b>根本不进这个列表</b>，Provider 想选也够不着。护栏是它之上的第二道保险。
      </p>
    </div>
  </div>
</template>

<style scoped>
.constraints {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.label {
  display: block;
  font-size: 0.72rem;
  color: var(--text-dim);
  margin-bottom: 0.3rem;
}
.count {
  color: var(--text-h);
}

.figure {
  display: flex;
  align-items: baseline;
  gap: 0.5rem;
}
.figure b {
  font-size: 1.7rem;
  line-height: 1;
  color: var(--text-h);
  font-weight: 600;
}
.spent {
  font-size: 0.74rem;
  color: var(--text-dim);
}

.track {
  margin-top: 0.35rem;
  height: 0.4rem;
  border-radius: 4px;
  background: var(--bg-sunken);
  border: 1px solid var(--border);
  overflow: hidden;
}
.fill {
  height: 100%;
  background: var(--text-dim);
  border-radius: 3px;
}

.rules ul {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}
.rules li {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.25rem;
}
.tag {
  flex: 0 0 4.2rem;
  font-size: 0.76rem;
  color: var(--text-h);
}
.chip {
  display: inline-flex;
  align-items: baseline;
  gap: 0.25rem;
  padding: 0.05em 0.4em;
  border-radius: 0.2rem;
  background: var(--bg-sunken);
  border: 1px solid var(--border);
  font-size: 0.72rem;
  color: var(--text-dim);
}
.chip em {
  font-style: normal;
  font-family: var(--mono);
  color: var(--text-h);
}

.hint {
  margin: 0.4rem 0 0;
  font-size: 0.7rem;
  line-height: 1.5;
}
</style>
