// ============================================================================
// 内置样例：三份日志，三个典型场景
//
// 直接从 `Docs/samples/` 引用，**不在 WebReplay 下放副本**。
// 这些 JSON 是 UE 侧导出的产物，复制一份进来就等于给同一份数据造了第二个真源——
// 正是把前端放进同一个仓库（D-21）想避免的事。
// 越过项目根目录的 import 靠 vite.config.ts 里的 `server.fs.allow` 放行。
//
// 页面首屏必须直接显示样例，不能是空白页 + 一个「请拖入文件」的提示：
// 面试官不会先去找一份 JSON 再拖进来。
// ============================================================================

import sampleLlm from '../../../Docs/samples/DecisionLog_Sample.json'
import sampleGuardrailRun from '../../../Docs/samples/DecisionLog_Guardrail_Run.json'
import sampleGuardrailIdeal from '../../../Docs/samples/DecisionLog_Guardrail_Ideal.json'

/**
 * 这份数据是怎么来的。
 *
 * **必须在页面上可见，不能只写在 JSON 的 `_note` 里**——`_note` 渲染不出来，
 * 而页面把每一份数据都摆成「本局概览 / runId / 层数」的样子，
 * 等于在暗示它是一局游戏。夹具被当成对局记录，是这个页面最容易犯的诚信错误。
 */
export type SampleKind =
  /** 玩家实打一局，FloorManager 整局结束时自动导出 */
  | 'realRun'
  /** 控制台脚本逐层调出来的决策，不是一局游戏 */
  | 'fixture'

export interface BuiltinSample {
  id: string
  /** 下拉框里显示的名字 */
  label: string
  kind: SampleKind
  /** 这一份要看的是什么 */
  hint: string
  /** 出处与限制。realRun 也要写清 Intent 是不是 LLM 现场产出的。 */
  provenance: string
  raw: unknown
}

export const BUILTIN_SAMPLES: BuiltinSample[] = [
  {
    id: 'guardrail-run',
    label: '真实对局 · 护栏拦下 2 次',
    kind: 'realRun',
    hint: '画像随对局演进（置信度 0.50→0.70），挑战等级 Stable → Pressure → Counter，两层被拦并降级。',
    provenance:
      '玩家实打三层，整局结束时自动导出，层号/画像/耗时/runId/时间戳原样未改。' +
      'Intent 由回放脚本注入而非 LLM 现场产出——构造的只有「AI 提出了什么」这一个输入，' +
      '此后护栏判定、查表出数值、降级、本地重新决策全部是真实代码跑出来的。',
    raw: sampleGuardrailRun,
  },
  {
    id: 'llm',
    label: '真实对局 · LLM 直采全绿',
    kind: 'realRun',
    hint: 'DeepSeek 现场产出，三层零拦截。看点：rawIntent 只有标签，decision 才有数值。',
    provenance:
      '真实对局导出（近战打法，LLM = DeepSeek deepseek-chat），仅替换了 runId 与时间戳。' +
      '台词是 LLM 现场生成的。',
    raw: sampleLlm,
  },
  {
    id: 'guardrail-ideal',
    label: '脚本测试 · 三道护栏全亮（理想情况）',
    kind: 'fixture',
    hint: '每层刻意只犯一条规：F1 规则互斥 / F2 超出预算 / F3 结构合法性。',
    provenance:
      '**这不是一局游戏**，是用 SHM.DumpDecisionAsync 手喂固定画像逐层跑出来的决策。' +
      '第 3 层在游戏里到不了（TotalFloors=3），四层画像完全相同（控制台把画像写死了）。' +
      '一局连拦三次且三道各不相同是刻意构造的理想情况——真实 LLM 撞出拦截是随机事件，' +
      '生产环境中概率很低。它证明的是三道护栏结构上都拦得住。',
    raw: sampleGuardrailIdeal,
  },
]

/**
 * 首屏默认载入哪一份。
 *
 * 选「真实对局 + 护栏拦截」：这个页面存在的唯一理由是让护栏的约束肉眼可见，
 * 而首屏就该是**既真实、又拦得住**的那一份。
 * 全绿对照和理想夹具留在下拉框里。
 */
export const DEFAULT_SAMPLE_ID = 'guardrail-run'
