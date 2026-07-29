<script setup lang="ts">
// 页面骨架：样例选择 + 出处横幅 + 概览（M1）+ 时间轴（M1）+ 单层详情（M2）。
// 详情里第 Ⅰ/Ⅱ 列（雷达图、约束）在 M3 补。
import { computed, ref, watch } from 'vue'
import RunHeader from './components/RunHeader.vue'
import FloorTimeline from './components/FloorTimeline.vue'
import FloorDetail from './components/FloorDetail.vue'
import { BUILTIN_SAMPLES, DEFAULT_SAMPLE_ID } from './samples'
import { parseDecisionLog } from './types/parseDecisionLog'

const sampleId = ref(DEFAULT_SAMPLE_ID)
const sample = computed(
  () => BUILTIN_SAMPLES.find((s) => s.id === sampleId.value) ?? BUILTIN_SAMPLES[0],
)

const result = computed(() => parseDecisionLog(sample.value.raw))

const selected = ref(0)
// 换样例后原来的层号可能不存在了，回到第一层。
// 不这么做的话详情区会空着，看起来像点击失效。
watch(result, (r) => {
  if (r.ok && !r.run.floors.some((f) => f.floorIndex === selected.value)) {
    selected.value = r.run.floors[0]?.floorIndex ?? 0
  }
})

const currentFloor = computed(() => {
  if (!result.value.ok) return undefined
  return result.value.run.floors.find((f) => f.floorIndex === selected.value)
})
</script>

<template>
  <div class="page">
    <div class="sampleBar">
      <label for="sample">样例</label>
      <select id="sample" v-model="sampleId">
        <option v-for="s in BUILTIN_SAMPLES" :key="s.id" :value="s.id">{{ s.label }}</option>
      </select>
      <span class="dim hint">{{ sample.hint }}</span>
    </div>

    <!--
      出处横幅。**必须在页面上可见**——JSON 里的 _note 渲染不出来，
      而这个页面把每一份数据都摆成「本局概览 / runId / 层数」的样子，
      等于在暗示它是一局游戏。夹具被当成对局记录是这里最容易犯的诚信错误。
    -->
    <p class="provenance" :class="sample.kind">
      <span class="tag">{{ sample.kind === 'realRun' ? '真实对局' : '脚本测试 · 非对局' }}</span>
      {{ sample.provenance }}
    </p>

    <template v-if="result.ok">
      <RunHeader :run="result.run" />

      <FloorTimeline
        :floors="result.run.floors"
        :selected="selected"
        @select="selected = $event"
      />

      <FloorDetail v-if="currentFloor" :floor="currentFloor" />
    </template>

    <section v-else class="errors">
      <h2>无法解析这份日志</h2>
      <ul>
        <li v-for="(e, i) in result.errors" :key="i">{{ e }}</li>
      </ul>
    </section>

    <details v-if="result.warnings.length" class="warnings">
      <summary>{{ result.warnings.length }} 条解析警告</summary>
      <ul>
        <li v-for="(w, i) in result.warnings" :key="i">{{ w }}</li>
      </ul>
    </details>
  </div>
</template>

<style scoped>
.page {
  max-width: 68rem;
  margin: 0 auto;
  padding: 1.5rem 1.25rem 4rem;
}

.sampleBar {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-bottom: 1.25rem;
  font-size: 0.85rem;
}
.sampleBar select {
  font: inherit;
  padding: 0.25rem 0.5rem;
  border-radius: 0.3rem;
  border: 1px solid var(--border-strong);
  background: var(--bg-panel);
  color: var(--text-h);
}
.hint {
  flex: 1 1 18rem;
  min-width: 0;
}

.provenance {
  margin: 0 0 1.25rem;
  padding: 0.6rem 0.85rem;
  border-radius: 0.4rem;
  border: 1px solid var(--border);
  background: var(--bg-sunken);
  font-size: 0.8rem;
  line-height: 1.55;
}
.provenance .tag {
  display: inline-block;
  margin-right: 0.4rem;
  padding: 0.05em 0.45em;
  border-radius: 0.2rem;
  font-weight: 600;
  white-space: nowrap;
}
.provenance.realRun .tag {
  color: var(--ok);
  background: var(--ok-bg);
}
/* 夹具用警示色：这是全页最不该被误读的一件事 */
.provenance.fixture {
  border-color: color-mix(in srgb, var(--warn) 45%, transparent);
  background: var(--warn-bg);
}
.provenance.fixture .tag {
  color: var(--warn);
  background: color-mix(in srgb, var(--warn) 20%, transparent);
}

.errors {
  border: 1px solid color-mix(in srgb, var(--bad) 40%, transparent);
  background: var(--bad-bg);
  border-radius: 0.5rem;
  padding: 1rem 1.25rem;
}
.errors h2 {
  font-size: 1rem;
  color: var(--bad);
}

.warnings {
  margin-top: 1.25rem;
  font-size: 0.82rem;
  color: var(--text-dim);
}
.warnings summary {
  cursor: pointer;
}
</style>
