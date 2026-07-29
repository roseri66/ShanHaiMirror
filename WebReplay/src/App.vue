<script setup lang="ts">
// 页面骨架：来源选择（内置样例 / 拖入文件）+ 出处横幅 + 概览 + 时间轴 + 单层详情。
import { computed, ref, watch } from 'vue'
import RunHeader from './components/RunHeader.vue'
import FloorTimeline from './components/FloorTimeline.vue'
import FloorDetail from './components/FloorDetail.vue'
import SourceBar from './components/SourceBar.vue'
import { BUILTIN_SAMPLES, DEFAULT_SAMPLE_ID } from './samples'
import { parseDecisionLog, type ParseResult } from './types/parseDecisionLog'
import { loadFromFile } from './types/loadSource'

const sampleId = ref(DEFAULT_SAMPLE_ID)
const sample = computed(
  () => BUILTIN_SAMPLES.find((s) => s.id === sampleId.value) ?? BUILTIN_SAMPLES[0],
)

/** 用户拖入的文件。为 null 时显示内置样例。 */
const loaded = ref<{ name: string; result: ParseResult } | null>(null)

const result = computed<ParseResult>(() =>
  loaded.value ? loaded.value.result : parseDecisionLog(sample.value.raw),
)

async function onFile(file: File) {
  loaded.value = { name: file.name, result: await loadFromFile(file) }
}

function backToSample() {
  loaded.value = null
}

/** 换来源时选中层可能不存在了 */
watch(sampleId, () => {
  loaded.value = null
})

const selected = ref(0)
// 换来源后原来的层号可能不存在了，回到第一层。
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
    <SourceBar
      :sample-id="sampleId"
      :loaded-name="loaded?.name ?? null"
      @update:sample-id="sampleId = $event"
      @file="onFile"
    />

    <!--
      出处横幅。**必须在页面上可见**——JSON 里的 _note 渲染不出来，
      而这个页面把每一份数据都摆成「本局概览 / runId / 层数」的样子，
      等于在暗示它是一局游戏。夹具被当成对局记录是这里最容易犯的诚信错误。

      用户自己拖进来的文件，出处我们无从知晓，所以只能如实说"不知道"——
      绝不能沿用上一份样例的标签，那会把别人的数据标成我们的真实对局。
    -->
    <p v-if="loaded" class="provenance user">
      <span class="tag">你载入的文件</span>
      {{ loaded.name }} —— 出处由你自己掌握，本页只负责按决策日志格式渲染它。
    </p>
    <p v-else class="provenance" :class="sample.kind">
      <span class="tag">{{ sample.kind === 'realRun' ? '真实对局' : '脚本测试 · 非对局' }}</span>
      {{ sample.provenance }}
    </p>

    <p v-if="!loaded" class="hint dim">{{ sample.hint }}</p>

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
      <!-- 载入失败不能变成死路：一定要有一条回到能看的东西的路 -->
      <p v-if="loaded" class="back">
        <button type="button" @click="backToSample">← 回到内置样例</button>
      </p>
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

.hint {
  margin: 0.5rem 0 0;
  font-size: 0.78rem;
  line-height: 1.5;
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
/* 用户自己的文件：中性色。我们不知道它的出处，就不该给它任何背书或警告 */
.provenance.user .tag {
  color: var(--slot-1);
  background: color-mix(in srgb, var(--slot-1) 15%, transparent);
}

.back {
  margin: 0.85rem 0 0;
}
.back button {
  font: inherit;
  font-size: 0.82rem;
  padding: 0.3rem 0.7rem;
  border-radius: 0.3rem;
  border: 1px solid currentColor;
  background: transparent;
  color: var(--bad);
  cursor: pointer;
}
.back button:hover {
  background: color-mix(in srgb, var(--bad) 12%, transparent);
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
