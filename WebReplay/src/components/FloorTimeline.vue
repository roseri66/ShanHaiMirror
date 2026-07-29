<script setup lang="ts">
// 层时间轴：横向卡片，点击切换下方详情。
// 每张卡片要在一瞥之内回答三件事：这层谁做的决策、拦没拦、降没降级。
import type { Floor } from '../types/decisionLog'
import { GUARD_ORDER, normalizeGuard } from '../types/decisionLog'
import { CHALLENGE_LEVEL_LABELS, GUARD_LABELS, PROVIDER_LABELS, labelOf } from '../types/labels'
import { formatElapsed } from '../types/runStats'

defineProps<{ floors: Floor[]; selected: number }>()
const emit = defineEmits<{ select: [floorIndex: number] }>()

function violationsOf(floor: Floor, guard: string) {
  return floor.validation.violations.filter((v) => normalizeGuard(v.guard) === guard)
}
</script>

<template>
  <nav class="timeline" aria-label="层时间轴">
    <button
      v-for="floor in floors"
      :key="floor.floorIndex"
      type="button"
      class="card"
      :class="{
        active: floor.floorIndex === selected,
        degraded: floor.trace.degraded,
      }"
      :aria-current="floor.floorIndex === selected"
      @click="emit('select', floor.floorIndex)"
    >
      <span class="top">
        <span class="floor">F{{ floor.floorIndex }}</span>
        <span class="badge provider" :data-provider="floor.trace.providerId">
          {{ labelOf(PROVIDER_LABELS, floor.trace.providerId) }}
        </span>
      </span>

      <span class="level">
        {{ labelOf(CHALLENGE_LEVEL_LABELS, floor.decision.challengeLevel) || '—' }}
      </span>

      <span class="lamps">
        <span
          v-for="g in GUARD_ORDER"
          :key="g"
          class="lamp"
          :class="{ hit: violationsOf(floor, g).length > 0 }"
          :title="
            violationsOf(floor, g).length
              ? labelOf(GUARD_LABELS, g) + '：' + violationsOf(floor, g).map((v) => v.detail).join('；')
              : labelOf(GUARD_LABELS, g) + '：通过'
          "
        />
        <span class="elapsed">{{ formatElapsed(floor.trace.elapsedMs) }}</span>
      </span>

      <span v-if="floor.trace.degraded" class="degradeTag" :title="floor.trace.degradeReason">
        已降级
      </span>
    </button>
  </nav>
</template>

<style scoped>
.timeline {
  display: flex;
  gap: 0.6rem;
  /* 卡片多了横向滚，但**只有这一条**允许横向滚动，页面本身不许（设计文档 M4） */
  overflow-x: auto;
  padding: 1rem 0;
  scrollbar-width: thin;
}

.card {
  flex: 0 0 auto;
  min-width: 9.5rem;
  display: flex;
  flex-direction: column;
  gap: 0.4rem;
  padding: 0.65rem 0.8rem;
  border: 1px solid var(--border);
  border-radius: 0.5rem;
  background: var(--bg-panel);
  color: inherit;
  font: inherit;
  text-align: left;
  cursor: pointer;
  transition: border-color 0.15s, transform 0.15s;
}
.card:hover {
  border-color: var(--border-strong);
}
.card:focus-visible {
  outline: 2px solid var(--text-h);
  outline-offset: 2px;
}
.card.active {
  border-color: var(--text-h);
  box-shadow: inset 0 0 0 1px var(--text-h);
}
/* 降级层左侧留一道竖条：扫一眼就知道哪几层没走成主路径 */
.card.degraded {
  border-left: 3px solid var(--bad);
}

.top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.5rem;
}
.floor {
  font-family: var(--mono);
  font-size: 1.05rem;
  font-weight: 600;
  color: var(--text-h);
}

/* Provider 徽章按"这层的决策有多接近主张"分层：LLM 最亮，导演关闭最淡 */
.provider {
  background: var(--bg-sunken);
  border-color: var(--border);
  color: var(--text-dim);
}
.provider[data-provider='Llm'] {
  color: var(--num);
  background: var(--num-bg);
  border-color: color-mix(in srgb, var(--num) 40%, transparent);
}
.provider[data-provider='Replay'] {
  color: var(--text);
  border-color: var(--border-strong);
}
.provider[data-provider='ObserveFloor'],
.provider[data-provider='Disabled'] {
  opacity: 0.65;
  background: transparent;
}

.level {
  font-size: 0.85rem;
  color: var(--text-h);
}

.lamps {
  display: flex;
  align-items: center;
  gap: 0.25rem;
}
.lamp {
  width: 0.5rem;
  height: 0.5rem;
  border-radius: 50%;
  background: var(--ok);
  flex: 0 0 auto;
}
.lamp.hit {
  background: var(--bad);
  /* 红灯要比绿灯更抓眼：这是整个页面最想让人看见的东西 */
  box-shadow: 0 0 0 2px var(--bad-bg);
}
.elapsed {
  margin-left: auto;
  font-family: var(--mono);
  font-size: 0.72rem;
  color: var(--text-dim);
}

.degradeTag {
  align-self: flex-start;
  font-size: 0.72rem;
  padding: 0.05em 0.4em;
  border-radius: 0.2rem;
  color: var(--bad);
  background: var(--bad-bg);
}
</style>
