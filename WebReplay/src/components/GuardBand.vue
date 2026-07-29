<script setup lang="ts">
// 四道护栏灯带 —— 夹在「想改」与「实改」之间，是这一屏的因果枢纽：
// 左边是 Provider 提出的，右边是实际生效的，中间这条解释了为什么两者不同。
import { computed, ref } from 'vue'
import type { Floor } from '../types/decisionLog'
import { guardBand } from '../types/floorView'
import { GUARD_LABELS, labelOf } from '../types/labels'

const props = defineProps<{ floor: Floor }>()

const band = computed(() => guardBand(props.floor))
const expanded = ref<string | null>(null)

function toggle(guard: string, hit: boolean) {
  // 只有亮红的灯有东西可展开；绿灯点开是一片空白，不如不响应
  if (!hit) return
  expanded.value = expanded.value === guard ? null : guard
}
</script>

<template>
  <div class="band">
    <ol class="lamps">
      <li v-for="lamp in band.lamps" :key="lamp.guard">
        <button
          type="button"
          class="lamp"
          :class="{ hit: lamp.hit, open: expanded === lamp.guard }"
          :disabled="!lamp.hit"
          :aria-expanded="lamp.hit ? expanded === lamp.guard : undefined"
          @click="toggle(lamp.guard, lamp.hit)"
        >
          <span class="dot" aria-hidden="true" />
          <span class="name">{{ labelOf(GUARD_LABELS, lamp.guard) }}</span>
          <!-- 状态不只靠颜色：绿灯写「通过」，红灯写拦截次数 -->
          <span class="state">{{ lamp.hit ? `拦 ${lamp.violations.length}` : '通过' }}</span>
        </button>

        <ul v-if="expanded === lamp.guard" class="details">
          <li v-for="(v, i) in lamp.violations" :key="i">{{ v.detail }}</li>
        </ul>
      </li>
    </ol>

    <!-- UE 侧加了第五道而前端没跟上时，这条违规不能被吞掉 -->
    <div v-if="band.unknown.length" class="unknown">
      <b>未知护栏</b>（本回放器还不认识，原样列出）
      <ul>
        <li v-for="(v, i) in band.unknown" :key="i">
          <code>{{ v.rawGuard }}</code> {{ v.detail }}
        </li>
      </ul>
    </div>
  </div>
</template>

<style scoped>
.band {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.lamps {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 0.35rem;
}

.lamp {
  display: flex;
  align-items: center;
  gap: 0.45rem;
  width: 100%;
  padding: 0.35rem 0.6rem;
  border: 1px solid var(--border);
  border-radius: 0.35rem;
  background: var(--bg-panel);
  color: inherit;
  font: inherit;
  font-size: 0.78rem;
  text-align: left;
  cursor: default;
}
.lamp.hit {
  cursor: pointer;
  border-color: color-mix(in srgb, var(--bad) 45%, transparent);
  background: var(--bad-bg);
}
.lamp.hit:hover,
.lamp.open {
  border-color: var(--bad);
}
.lamp:focus-visible {
  outline: 2px solid var(--text-h);
  outline-offset: 2px;
}

.dot {
  width: 0.55rem;
  height: 0.55rem;
  border-radius: 50%;
  background: var(--ok);
  flex: 0 0 auto;
}
.lamp.hit .dot {
  background: var(--bad);
  box-shadow: 0 0 0 3px var(--bad-bg);
}

.name {
  white-space: nowrap;
}
.state {
  margin-left: auto;
  font-size: 0.72rem;
  color: var(--text-dim);
}
.lamp.hit .state {
  color: var(--bad);
  font-weight: 600;
}

.details {
  margin: 0.3rem 0 0;
  padding: 0.4rem 0.6rem 0.4rem 1.4rem;
  border-left: 2px solid var(--bad);
  font-size: 0.76rem;
  line-height: 1.5;
  color: var(--text);
}

.unknown {
  padding: 0.5rem 0.65rem;
  border: 1px solid color-mix(in srgb, var(--warn) 45%, transparent);
  background: var(--warn-bg);
  border-radius: 0.35rem;
  font-size: 0.75rem;
}
.unknown ul {
  margin: 0.25rem 0 0;
  padding-left: 1.1rem;
}
</style>
