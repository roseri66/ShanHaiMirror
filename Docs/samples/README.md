# 样例数据

三份日志，三个典型场景。格式契约见
[`SHMDecisionLogFormat.h`](../../UnrealProject/Source/ShanHaiMirror/Director/SHMDecisionLogFormat.h)。

| 文件 | 是什么 | 决策来源 | 护栏 |
|---|---|---|---|
| [`DecisionLog_Sample.json`](DecisionLog_Sample.json) | 真实对局 | **LLM 直采** | 零拦截 |
| [`DecisionLog_Guardrail_Run.json`](DecisionLog_Guardrail_Run.json) | 真实对局 | 回放脚本注入 | **拦 2 次** |
| [`DecisionLog_Guardrail_Ideal.json`](DecisionLog_Guardrail_Ideal.json) | **控制台脚本测试，非对局** | 回放脚本注入 | **拦 3 次（理想情况）** |

> **每份文件顶部都有 `_kind` / `_note` 字段说明它是怎么来的。**
> 三份都是引擎真实跑出来的产物 —— 没有任何一份是手写的 JSON。

---

## 1. `DecisionLog_Sample.json` —— 真实对局 · LLM 直采

近战打法，LLM = DeepSeek `deepseek-chat`，仅替换了 runId 与时间戳。

| 层 | 决策来源 | 耗时 | 想改 → 实改 | 白泽说 |
|---|---|---|---|---|
| F0 | ObserveFloor | — | （不调整） | 第一层。我只是在看。 |
| F1 | **Llm** | 3721ms | MeleeDamage/medium → ×0.80 | 近战猛攻？试试砍穿这层铁壁。 |
| F2 | **Llm** | 2804ms | Heal + Ammo/medium → ×0.70 ×0.70 | 近战莽夫？试试远程消耗加治疗封锁，看你怎么近身。 |

台词是 LLM 现场生成的，针对玩家实际的近战打法。三层护栏零拦截，决策全部直采。

**这份证明的是**：链路在顺利路径下能跑通，LLM 确实承担了"选择与表达"。

---

## 2. `DecisionLog_Guardrail_Run.json` —— 真实对局 · 护栏拦下 2 次

玩家实打三层，整局结束时由 `FloorManager` 自动导出。**层号、画像、耗时、runId、
时间戳全部原样未改。**

| 层 | 画像 | 挑战等级 | 护栏 | 白泽说 |
|---|---|---|---|---|
| F0 | 置信度 0.50 · 未定型 | Stable | ✓ | 第一层。我只是在看。 |
| F1 | 置信度 0.50 · 先锋 | **Pressure** | ✗ **规则互斥** → 降级 | 你走得太顺了。镜中的试炼，该加深了。 |
| F2 | 置信度 **0.70** · 先锋 | **Counter** | ✗ **超出预算** → 降级 | 刀锋所及皆是坦途？这一层试试够不到的敌人。 |

**这份证明的是**：护栏在真实对局中确实会拦截并降级，且游戏不因此中断。
同时能看到画像随对局演进（置信度 0.50 → 0.70）、挑战等级随之递进
（Stable → Pressure → Counter），这正是"观察 → 试探 → 反制"。

> **注意 `trace.providerId = Replay`**：本局的 Intent 由回放脚本注入，
> 不是 LLM 现场产出的。构造的只有"AI 提出了什么"这一个输入；
> 此后护栏判定、查表出数值、降级、本地重新决策、日志导出**全部是真实代码跑出来的**。
> 所以它证明的是「护栏会拦下危险意图」，而不是「LLM 产出过危险意图」。

### 顺带能看见的一件事

被拦下之后，**台词也一并换成本地库的了**。比较 F1：

- `rawIntent.narration` → 「你的箭袋会更浅，你的箭也会更钝。」（被拦掉的那句）
- `decision.narration` → 「你走得太顺了。镜中的试炼，该加深了。」（玩家实际听到的）

护栏不只拦住数值，连表达一起接管。

---

## 3. `DecisionLog_Guardrail_Ideal.json` —— 控制台脚本测试 · 三道全亮

**这不是一局游戏。** 用 `SHM.DumpDecisionAsync` 手喂固定画像逐层跑决策，
Intent 由回放脚本注入。存在的唯一目的：证明三道护栏各自拦得住，结构成立。

| 层 | 预算 | 注入的 Intent | 拦截 |
|---|---|---|---|
| F1 | 30 | Ammo/light + RangedDamage/light（Σcost 20，预算够） | ✗ **规则互斥** |
| F2 | 55 | 四条 medium（Σcost 80） | ✗ **挑战预算** |
| F3 | 80 | 敌人权重和 0.90 | ✗ **结构合法性** |

每层刻意只犯一条规，便于逐道核对。

### 必须知道的两点限制

1. **第 3 层在游戏里到不了。** `FloorManager::TotalFloors = 3`，`FloorIndex >= 3`
   即结束（`SHMFloorManager.cpp:174`）。真实对局只有 F0/F1/F2。这里的 F3 是控制台
   单独调出来的一次决策，**不代表任何一局的第四层**。同理四层画像完全相同，
   因为 `DumpDecisionAsync` 把画像写死了。
2. **一局连拦三次且三道各不相同，是刻意构造的理想情况。** 真实 LLM 撞出拦截是
   随机事件（实测三次 DeepSeek 调用撞到一次 Conflict），生产环境中概率很低。
   要看真实对局里的护栏拦截，看上面第 2 份。

---

## 怎么读这些文件

每层的 `rawIntent` 与 `decision` 并排对比：

- `rawIntent.ruleIntents` —— **护栏前**，只有 `tag` 和 `level`，**没有任何数值**
- `decision.ruleModifiers` —— **护栏后**，出现 `multiplier`

数值只在护栏之后产生，这条架构主张在数据里肉眼可见。

`validation.violations` 记录哪道护栏拦下了什么，`trace` 记录这条决策出自
Local / Llm / Replay、是否降级、耗时多久。

> **自己再生成一份**：打完整一局自动导出到 `UnrealProject/Saved/DecisionLogs/Run_*.json`，
> 或游戏中控制台 `SHM.ExportDecisionLog`。文件为 UTF-8，任意 JSON 工具可直读。
>
> 要复现第 3 份的回放脚本：`git show 5988043:UnrealProject/Data/ReplayScripts/Guardrail.json`，
> 存到 `UnrealProject/Data/ReplayScripts/` 下并设环境变量 `SHM_REPLAY_SCRIPT` 指向它。
