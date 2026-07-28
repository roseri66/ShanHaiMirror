// ============================================================================
// 内置样例
//
// 直接从 `Docs/samples/` 引用，**不在 WebReplay 下放副本**。
// 这些 JSON 是 UE 侧 `SHM.ExportDecisionLog` 的产物，复制一份进来就等于
// 给同一份数据造了第二个真源——正是把前端放进同一个仓库（D-21）想避免的事。
// 越过项目根目录的 import 靠 vite.config.ts 里的 `server.fs.allow` 放行。
//
// 页面首屏必须直接显示样例，不能是空白页 + 一个「请拖入文件」的提示：
// 面试官不会先去找一份 JSON 再拖进来。
// ============================================================================

import sampleGreen from '../../../Docs/samples/DecisionLog_Sample.json'
import sampleGuardrail from '../../../Docs/samples/DecisionLog_Guardrail.json'

export interface BuiltinSample {
  id: string
  /** 下拉框里显示的名字 */
  label: string
  /** 一句话说明这份样例要看的是什么 */
  hint: string
  raw: unknown
}

export const BUILTIN_SAMPLES: BuiltinSample[] = [
  {
    id: 'green',
    label: '真实对局（近战三层）',
    hint: 'DeepSeek 直采，三层全部通过护栏。看点：rawIntent 只有标签、decision 才有数值。',
    raw: sampleGreen,
  },
  {
    id: 'guardrail',
    label: '护栏拦截（四层）',
    hint: '回放 Provider 喂必被拒的 Intent，确定性复现三道拦截：F1 规则互斥 / F2 超预算 / F3 结构非法。',
    raw: sampleGuardrail,
  },
]

/**
 * 首屏默认载入哪一份。
 *
 * 选护栏样例而不是全绿样例：这个页面存在的唯一理由是让「护栏在约束 LLM」肉眼可见，
 * 首屏就该是它最想证明的那件事。全绿样例作为对照留在下拉框里。
 */
export const DEFAULT_SAMPLE_ID = 'guardrail'
