<script setup lang="ts">
// 单层详情 · 第 Ⅲ 列「想改 → 护栏 → 实改」—— 整个页面的价值所在。
//
// 布局刻意做成三栏：左（护栏前）· 中（四道灯带）· 右（护栏后）。
// **左表没有倍率列，右表有且高亮**——「数值只在护栏之后产生」这条架构主张
// 不靠文字说明，靠两张表的列数差摆在那里。
//
// 上方是第 Ⅰ 列「我看到的」（画像）与第 Ⅱ 列「约束」（预算 + 候选集）——
// 顺序即链路顺序：① 观察 → ③ 约束 → ④ 选择 → ⑤ 护栏 → ⑥ 数值。
import { computed } from 'vue'
import GuardBand from './GuardBand.vue'
import WeightBars from './WeightBars.vue'
import ProfileRadar from './ProfileRadar.vue'
import ConstraintPanel from './ConstraintPanel.vue'
import type { Floor } from '../types/decisionLog'
import { ruleComparison } from '../types/floorView'
import { degradeKind, formatMultiplier, narrationPair } from '../types/runStats'
import {
  CHALLENGE_LEVEL_LABELS,
  PROVIDER_LABELS,
  RULE_LEVEL_LABELS,
  RULE_TAG_LABELS,
  labelOf,
} from '../types/labels'

const props = defineProps<{ floor: Floor }>()

const rules = computed(() => ruleComparison(props.floor))
const narration = computed(() => narrationPair(props.floor))
const kind = computed(() => degradeKind(props.floor))

/** 观察层与导演关闭层压根没走 Provider，"护栏前"是空的，不该摆出对照的架势 */
const consultedProvider = computed(
  () => props.floor.rawIntent.ruleIntents.length > 0 || props.floor.rawIntent.narration !== '',
)
</script>

<template>
  <section class="detail">
    <header class="head">
      <h2>F{{ floor.floorIndex }} · 想改 → 护栏 → 实改</h2>
      <span class="badge provider" :data-provider="floor.trace.providerId">
        {{ labelOf(PROVIDER_LABELS, floor.trace.providerId) }}
      </span>
      <span class="badge level">
        {{ labelOf(CHALLENGE_LEVEL_LABELS, floor.decision.challengeLevel) || '—' }}
      </span>
    </header>

    <!--
      降级横幅。两种降级的含义完全不同，必须分开说：
      护栏拒绝时左列是被拦下的原件（有对照价值）；
      Provider 交不出结果时 DirectorCore 直接拿本地 Intent 走后续流程，
      左列已经是本地的了，没有原件可对照。
    -->
    <p v-if="floor.trace.degraded" class="degrade">
      <b>已降级</b>{{ floor.trace.degradeReason }}
      <span class="kind">{{
        kind === 'rejected'
          ? '护栏拒绝后由本地 Provider 重新决策 —— 左列是被拦下的原件'
          : 'Provider 未交出结果，左列已是本地产物，无原件可对照'
      }}</span>
    </p>

    <!-- Ⅰ 我看到的 · Ⅱ 约束 —— 摆在对照之前，因为链路顺序就是先观察再约束 -->
    <div class="inputs">
      <section>
        <h3>Ⅰ 我看到的 · 画像</h3>
        <ProfileRadar :profile="floor.profile" />
      </section>
      <section>
        <h3>Ⅱ 约束 · 候选集在这里就收敛了</h3>
        <ConstraintPanel :floor="floor" />
      </section>
    </div>

    <h3 class="sectionTitle">Ⅲ 想改 → 护栏 → 实改</h3>

    <div class="compare">
      <!-- 左：护栏前。**没有倍率列**，这是刻意的 -->
      <div class="side">
        <h3>护栏前 · Provider 提出</h3>
        <p v-if="!consultedProvider" class="empty dim">本层未调用 Provider</p>
        <p v-else-if="!rules.before.length" class="empty dim">未提出任何规则调整</p>
        <table v-else>
          <thead>
            <tr><th>规则</th><th>强度</th></tr>
          </thead>
          <tbody>
            <tr v-for="(b, i) in rules.before" :key="i" :class="b.status">
              <td>{{ labelOf(RULE_TAG_LABELS, b.rule.tag) }}</td>
              <td>{{ labelOf(RULE_LEVEL_LABELS, b.rule.level) }}</td>
            </tr>
          </tbody>
        </table>
        <p v-if="rules.before.some((b) => b.status === 'dropped')" class="tag dropped">
          划掉的没有生效
        </p>
      </div>

      <!-- 中：因果枢纽 -->
      <div class="middle">
        <GuardBand :floor="floor" />
      </div>

      <!-- 右：护栏后。倍率列在此首次出现并高亮 -->
      <div class="side">
        <h3>护栏后 · 实际生效</h3>
        <p v-if="!rules.after.length" class="empty dim">无规则调整</p>
        <table v-else>
          <thead>
            <tr><th>规则</th><th>强度</th><th class="num">倍率</th><th class="cost">代价</th></tr>
          </thead>
          <tbody>
            <tr v-for="(a, i) in rules.after" :key="i" :class="a.status">
              <td>{{ labelOf(RULE_TAG_LABELS, a.mod.tag) }}</td>
              <td>{{ labelOf(RULE_LEVEL_LABELS, a.mod.level) }}</td>
              <td class="num">{{ formatMultiplier(a.mod.multiplier) }}</td>
              <td class="cost">{{ a.mod.cost }}</td>
            </tr>
          </tbody>
        </table>
        <p v-if="rules.after.some((a) => a.status === 'added')" class="tag added">
          降级后新产生的
        </p>
        <p v-else-if="rules.after.length" class="tag kept">意图原样通过</p>
      </div>
    </div>

    <p class="claim">
      左表没有「倍率」列，右表才有 —— <b>数值只在护栏之后产生</b>。
      Provider 的输出在类型上就装不下数值（<code>FDirectorIntent</code>），
      倍率由第 ⑥ 步查规则表得到。
    </p>

    <section class="block">
      <h3>敌人配比</h3>
      <WeightBars :floor="floor" />
    </section>

    <section class="block">
      <h3>白泽说</h3>
      <div v-if="narration.rejected" class="narrations">
        <blockquote class="rejected">
          <span class="who">被拦下的那句</span>{{ narration.rejected }}
        </blockquote>
        <blockquote class="actual">
          <span class="who">玩家实际听到的（本地）</span>{{ narration.actual }}
        </blockquote>
      </div>
      <blockquote v-else-if="narration.actual">{{ narration.actual }}</blockquote>
      <p v-else class="empty dim">本层无台词</p>

      <p v-if="floor.decision.reason" class="reason dim">{{ floor.decision.reason }}</p>
    </section>
  </section>
</template>

<style scoped>
.detail {
  border: 1px solid var(--border);
  border-radius: 0.5rem;
  padding: 1rem 1.15rem 1.25rem;
  background: var(--bg-panel);
}

.head {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-bottom: 0.85rem;
}
h2 {
  font-size: 1.05rem;
  margin-right: 0.25rem;
}
h3 {
  font-size: 0.8rem;
  color: var(--text-dim);
  font-weight: 500;
  margin: 0 0 0.4rem;
}

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
.level {
  background: var(--bg-sunken);
  border-color: var(--border);
  color: var(--text-h);
}

.degrade {
  margin: 0 0 0.9rem;
  padding: 0.55rem 0.8rem;
  border-radius: 0.35rem;
  background: var(--bad-bg);
  border: 1px solid color-mix(in srgb, var(--bad) 40%, transparent);
  color: var(--bad);
  font-size: 0.82rem;
}
.degrade b {
  margin-right: 0.4rem;
}
.degrade .kind {
  display: block;
  margin-top: 0.15rem;
  font-size: 0.75rem;
  opacity: 0.9;
}

/* Ⅰ/Ⅱ 两列：画像 + 约束 */
.inputs {
  display: grid;
  grid-template-columns: minmax(0, 17rem) minmax(0, 1fr);
  gap: 1.25rem 1.75rem;
  padding-bottom: 1.15rem;
  margin-bottom: 1.15rem;
  border-bottom: 1px solid var(--border);
}
@media (max-width: 46rem) {
  .inputs {
    grid-template-columns: 1fr;
  }
}

.sectionTitle {
  margin-bottom: 0.7rem;
}

/* 三栏：左表 · 灯带 · 右表。窄屏堆叠，灯带落到中间仍保持因果顺序 */
.compare {
  display: grid;
  grid-template-columns: 1fr minmax(9.5rem, auto) 1fr;
  gap: 1rem;
  align-items: start;
}
@media (max-width: 52rem) {
  .compare {
    grid-template-columns: 1fr;
  }
}

.side {
  min-width: 0;
}
.middle {
  padding-top: 1.35rem;
}

table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.8rem;
}
th,
td {
  padding: 0.3rem 0.45rem;
  text-align: left;
  border-bottom: 1px solid var(--border);
}
th {
  font-weight: 500;
  color: var(--text-dim);
  font-size: 0.72rem;
}
td {
  color: var(--text-h);
}

/* 倍率列：整个页面的主张落在这一列上，所以它高亮 */
th.num,
td.num {
  text-align: right;
  font-family: var(--mono);
  color: var(--num);
  background: var(--num-bg);
  font-weight: 600;
}
th.cost,
td.cost {
  text-align: right;
  color: var(--text-dim);
  font-size: 0.75rem;
}

/* 被丢弃的意图划掉：整列划掉 = 护栏把这套整个换了 */
tr.dropped td {
  text-decoration: line-through;
  text-decoration-color: color-mix(in srgb, var(--bad) 60%, transparent);
  color: var(--text-dim);
}

.tag {
  margin: 0.35rem 0 0;
  font-size: 0.72rem;
}
.tag.dropped {
  color: var(--bad);
}
.tag.added {
  color: var(--warn);
}
.tag.kept {
  color: var(--ok);
}

.empty {
  margin: 0;
  font-size: 0.8rem;
}

.claim {
  margin: 1rem 0 0;
  padding: 0.6rem 0.8rem;
  border-radius: 0.35rem;
  background: var(--bg-sunken);
  border-left: 3px solid var(--num);
  font-size: 0.8rem;
  line-height: 1.6;
}

.block {
  margin-top: 1.25rem;
  padding-top: 1rem;
  border-top: 1px solid var(--border);
}

.narrations {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(15rem, 1fr));
  gap: 0.9rem;
}
blockquote {
  margin: 0;
  padding-left: 0.85rem;
  border-left: 3px solid var(--border-strong);
  color: var(--text-h);
  font-size: 0.92rem;
}
.narrations .who {
  display: block;
  font-size: 0.7rem;
  color: var(--text-dim);
  margin-bottom: 0.15rem;
}
.narrations .rejected {
  border-left-color: var(--bad);
  color: var(--text-dim);
  text-decoration: line-through;
  text-decoration-color: color-mix(in srgb, var(--bad) 50%, transparent);
}
.narrations .actual {
  border-left-color: var(--ok);
}
.reason {
  margin: 0.6rem 0 0;
  font-size: 0.78rem;
}
</style>
