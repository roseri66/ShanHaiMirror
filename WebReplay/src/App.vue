<script setup lang="ts">
// M0：数据层跑通的最小验证页。真正的界面在 M1（RunHeader + FloorTimeline）建起来。
// 此刻这一页存在的意义只有一个：证明「载入 → 解析 → 拿到结构化数据」这条链路是通的，
// 且跨仓库根目录 import Docs/samples/*.json 在真实 Vite 管线里能解析。
import { computed } from 'vue'
import { BUILTIN_SAMPLES, DEFAULT_SAMPLE_ID } from './samples'
import { parseDecisionLog } from './types/parseDecisionLog'
import { GUARD_ORDER } from './types/decisionLog'
import { CHALLENGE_LEVEL_LABELS, GUARD_LABELS, labelOf } from './types/labels'

const sample = BUILTIN_SAMPLES.find((s) => s.id === DEFAULT_SAMPLE_ID) ?? BUILTIN_SAMPLES[0]
const result = computed(() => parseDecisionLog(sample.raw))

// 设计文档的 M0 验收标准就是「控制台打出解析结果」
console.log('[M0] 解析结果', result.value)
</script>

<template>
  <main>
    <h1>山海镜 · 决策回放器</h1>
    <p class="sub">M0 · 数据层。界面在 M1 起搭。</p>

    <template v-if="result.ok">
      <p>
        <code>{{ result.run.runId }}</code> ·
        {{ result.run.floors.length }} / {{ result.run.totalFloors }} 层 ·
        警告 {{ result.warnings.length }} 条
      </p>

      <table>
        <thead>
          <tr>
            <th>层</th>
            <th>出自</th>
            <th>挑战等级</th>
            <th>护栏前（无数值）</th>
            <th>护栏后（有数值）</th>
            <th>护栏</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="f in result.run.floors" :key="f.floorIndex">
            <td>F{{ f.floorIndex }}</td>
            <td>{{ f.trace.providerId }}</td>
            <td>{{ labelOf(CHALLENGE_LEVEL_LABELS, f.rawIntent.challengeLevel) }}</td>
            <td>
              <span v-if="!f.rawIntent.ruleIntents.length" class="dim">未提出调整</span>
              <span v-for="r in f.rawIntent.ruleIntents" :key="r.tag + r.level" class="chip">
                {{ r.tag }}/{{ r.level }}
              </span>
            </td>
            <td>
              <span v-if="!f.decision.ruleModifiers.length" class="dim">无调整</span>
              <span v-for="m in f.decision.ruleModifiers" :key="m.tag + m.level" class="chip num">
                {{ m.tag }}/{{ m.level }} ×{{ m.multiplier.toFixed(2) }}
              </span>
            </td>
            <td>
              <span
                v-for="g in GUARD_ORDER"
                :key="g"
                class="lamp"
                :class="{ hit: f.validation.violations.some((v) => v.guard === g) }"
                :title="labelOf(GUARD_LABELS, g)"
              />
            </td>
          </tr>
        </tbody>
      </table>
    </template>

    <template v-else>
      <h2>解析失败</h2>
      <ul>
        <li v-for="(e, i) in result.errors" :key="i">{{ e }}</li>
      </ul>
    </template>
  </main>
</template>

<style scoped>
main {
  max-width: 60rem;
  margin: 2rem auto;
  padding: 0 1rem;
  font-family: system-ui, sans-serif;
  text-align: left;
}
.sub {
  opacity: 0.6;
}
table {
  border-collapse: collapse;
  width: 100%;
  font-size: 0.85rem;
}
th,
td {
  border: 1px solid currentColor;
  border-color: color-mix(in srgb, currentColor 20%, transparent);
  padding: 0.4rem 0.6rem;
  text-align: left;
  vertical-align: top;
}
.chip {
  display: inline-block;
  padding: 0.1rem 0.4rem;
  margin: 0.1rem;
  border-radius: 0.25rem;
  background: color-mix(in srgb, currentColor 10%, transparent);
}
/* 「数值只在护栏后产生」——右列用不同底色，让这件事一眼看得见 */
.chip.num {
  background: color-mix(in srgb, #d4a017 25%, transparent);
}
.dim {
  opacity: 0.45;
}
.lamp {
  display: inline-block;
  width: 0.6rem;
  height: 0.6rem;
  margin-right: 0.2rem;
  border-radius: 50%;
  background: #3fb950;
}
.lamp.hit {
  background: #f85149;
}
</style>
