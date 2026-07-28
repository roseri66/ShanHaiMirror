<script setup lang="ts">
// M1：概览 + 时间轴 + 点击切层。
// 单层详情（M2 的护栏前后对照）还没做，此处先占位并把选中层的关键字段摊平显示，
// 保证"点了有反应、数据确实换了"这件事现在就能验收。
import { computed, ref, watch } from 'vue'
import RunHeader from './components/RunHeader.vue'
import FloorTimeline from './components/FloorTimeline.vue'
import { BUILTIN_SAMPLES, DEFAULT_SAMPLE_ID } from './samples'
import { parseDecisionLog } from './types/parseDecisionLog'
import { degradeKind, formatMultiplier, formatPercent, narrationPair } from './types/runStats'

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

const narration = computed(() =>
  currentFloor.value ? narrationPair(currentFloor.value) : { actual: '' },
)
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

      <section v-if="currentFloor" class="detail">
        <h2>F{{ currentFloor.floorIndex }} 详情</h2>
        <p class="placeholder dim">
          护栏前后对照、雷达图、白泽台词在 M2 / M3 建起来。以下是本层的原始字段，
          用于验证"点击切层确实换了数据"。
        </p>

        <div class="cols">
          <div>
            <h3>护栏前 · rawIntent</h3>
            <p v-if="!currentFloor.rawIntent.ruleIntents.length" class="dim">未提出调整</p>
            <p v-else class="chips">
              <span
                v-for="r in currentFloor.rawIntent.ruleIntents"
                :key="r.tag + r.level"
                class="chip"
              >{{ r.tag }} · {{ r.level }}</span>
            </p>
          </div>

          <div>
            <h3>护栏后 · decision</h3>
            <p v-if="!currentFloor.decision.ruleModifiers.length" class="dim">无调整</p>
            <p v-else class="chips">
              <span
                v-for="m in currentFloor.decision.ruleModifiers"
                :key="m.tag + m.level"
                class="chip num"
              >{{ m.tag }} · {{ m.level }} <b>{{ formatMultiplier(m.multiplier) }}</b></span>
            </p>
          </div>

          <div>
            <h3>敌人配比</h3>
            <p class="chips">
              <span
                v-for="(w, tag) in currentFloor.decision.enemyWeights"
                :key="tag"
                class="chip"
              >{{ tag }} {{ formatPercent(w) }}</span>
            </p>
          </div>
        </div>

        <ul v-if="currentFloor.validation.violations.length" class="violations">
          <li v-for="(v, i) in currentFloor.validation.violations" :key="i">
            <b>{{ v.guard }}</b> {{ v.detail }}
          </li>
        </ul>

        <p v-if="currentFloor.trace.degraded" class="degradeBanner">
          已降级：{{ currentFloor.trace.degradeReason }}
          <span class="kindNote">
            {{
              degradeKind(currentFloor) === 'rejected'
                ? '（护栏拒绝 · 上方 rawIntent 是被拦下的原件）'
                : '（Provider 无输出 · rawIntent 已是本地产物，无原件可对照）'
            }}
          </span>
        </p>

        <!--
          降级后台词会被换成本地库的，所以两句都要留：
          只显示 decision 会把本地生成的句子当成 LLM 说的话，
          只显示 rawIntent 又会显示一句玩家其实没听到的话。
        -->
        <div v-if="narration.rejected" class="narrations">
          <blockquote class="rejected">
            <span class="who">被拦下的那句</span>
            {{ narration.rejected }}
          </blockquote>
          <blockquote class="actual">
            <span class="who">玩家实际听到的（本地）</span>
            {{ narration.actual }}
          </blockquote>
        </div>
        <blockquote v-else-if="narration.actual">
          {{ narration.actual }}
        </blockquote>
      </section>
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

.detail {
  border: 1px solid var(--border);
  border-radius: 0.5rem;
  padding: 1rem 1.15rem;
  background: var(--bg-panel);
}
.detail h2 {
  font-size: 1.05rem;
}
.placeholder {
  font-size: 0.82rem;
  margin: 0.35rem 0 1rem;
}

.cols {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(15rem, 1fr));
  gap: 1rem 1.5rem;
}
.cols h3 {
  font-size: 0.82rem;
  color: var(--text-dim);
  font-weight: 500;
  margin-bottom: 0.4rem;
}
.chips {
  display: flex;
  flex-wrap: wrap;
  gap: 0.3rem;
  margin: 0;
}
/* 数值只在护栏后产生——右列用不同底色让它一眼可见 */
.chip.num {
  color: var(--num);
  background: var(--num-bg);
  border-color: color-mix(in srgb, var(--num) 40%, transparent);
}

.violations {
  margin: 1rem 0 0;
  padding-left: 1.1rem;
  font-size: 0.85rem;
  color: var(--bad);
}

.degradeBanner {
  margin: 0.75rem 0 0;
  padding: 0.5rem 0.75rem;
  border-radius: 0.35rem;
  background: var(--bad-bg);
  border: 1px solid color-mix(in srgb, var(--bad) 35%, transparent);
  color: var(--bad);
  font-size: 0.85rem;
}

blockquote {
  margin: 1rem 0 0;
  padding-left: 0.9rem;
  border-left: 3px solid var(--border-strong);
  color: var(--text-h);
  font-size: 0.95rem;
}

.kindNote {
  display: block;
  font-size: 0.78rem;
  opacity: 0.85;
}

.narrations {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(16rem, 1fr));
  gap: 1rem;
}
.narrations .who {
  display: block;
  font-size: 0.72rem;
  color: var(--text-dim);
  margin-bottom: 0.15rem;
}
/* 被拦下的那句用删除线语义的暗色，实际播出的那句正常 —— 一眼分清谁是谁 */
.narrations .rejected {
  border-left-color: var(--bad);
  color: var(--text-dim);
  text-decoration: line-through;
  text-decoration-color: color-mix(in srgb, var(--bad) 50%, transparent);
}
.narrations .actual {
  border-left-color: var(--ok);
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
