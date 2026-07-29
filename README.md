# 山海镜 ShanHaiMirror

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**一个 AI 导演系统的架构实现。** UE 5.5 · C++ · 单人开发。

这个仓库要展示的不是一个游戏，是一套**在游戏运行时约束 LLM 做决策**的架构：LLM 负责"选择与表达"，代码负责"计算与安全"，两者的边界由类型系统而非纪律来保证。断网时系统完整降级，玩家无感知。

> **▶ [打开决策回放器](https://roseri66.github.io/ShanHaiMirror/)** —— 不用装引擎，点开就能看见
> 「LLM 想改什么 → 四道护栏拦没拦 → 实际改了什么」。用的是真实对局导出的决策日志，
> 也可以把你自己的 `DecisionLog_*.json` 拖进去。

> 视觉是引擎 primitive + 纯色材质。**本项目不包含美术资源，视觉不在评估范围内。**

---

## 技术主张

传统动态难度（DDA）改的是**数值**——敌人变肉、伤害变高。这个系统改的是**规则结构**，并且**定向针对玩家当前的 Build**，再用自然语言解释原因。

难点不在"让 LLM 做决策"，在于**让 LLM 的决策在一个实时游戏里可信**：它可能超时、可能返回非法 JSON、可能给出破坏平衡的组合、可能连续三层针对同一个弱点把玩家逼死。以下四条不变量是整个设计的核心。

### 四个不变量

**① 数值只在决策链路的第 ⑥ 步产生。**
LLM 永远看不到、也改不了任何数值。它只在标签空间里工作——输出 `{ "tag": "Rule.Ammo", "level": "medium" }`，由 C++ 查 `DT_Rule` 映射成 `×0.75`。
这条不变量由**类型系统强制**，不靠约定：

```cpp
// Provider 的输出：只有标签和等级，没有数值
USTRUCT() struct FDirectorIntent
{
    EChallengeLevel          ChallengeLevel;
    TMap<FGameplayTag,float> EnemyWeights;
    TArray<FRuleIntent>      RuleIntents;   // { Rule.Ammo, "medium" }
    FString                  Narration;
};

// 玩法层的输入：已查表、已过护栏、带具体数值
// 只能由 DirectorCore 产出——Provider 在编译期就构造不出它
USTRUCT() struct FDirectorDecision
{
    TArray<FRuleModifier>    RuleModifiers; // 已含 Multiplier
    FDirectorTrace           Trace;         // 决策溯源
    // ...
};
```

**② LLM 这一步可以整体失败。**
三个 Provider 实现同一接口，失败逐级降级到本地规则表，日志留痕。**本地 Provider 单独就是一个完整可玩的游戏**——LLM 是可拔插的增强层，不是依赖。拔掉网线，游戏照常运转。

**③ 画像分析是纯函数。**
`ProfileAnalyzer` 无副作用、无引擎依赖、不碰随机数。相同输入必定得到相同画像。这是它能被单测的前提，也是断网可跑的前提。

**④ 不信任 LLM 输出是设计前提，不是补丁。**
约束在第 ③ 步就已收敛（LLM 拿到的候选集本身是安全的），即便如此第 ⑤ 步仍然全量校验四道护栏。

---

## 决策链路

一层游戏的完整生命周期：

```text
战斗中  Combat ──event──> EventBus ──> BehaviorRecorder（实时累积）
   │
   ▼ OnFloorFinished
① OBSERVE    定稿 ──> FFloorBehaviorSnapshot
   ▼
② ANALYZE    ProfileAnalyzer(Snapshot, History) ──> FPlayerProfile    【纯函数·可单测】
   ▼
③ CONSTRAIN  DirectorCore 构建 FDirectorContext                       【约束在此收敛】
             ├ ChallengeBudget(FloorIndex)   预算上限
             ├ AvailableRules[]              已剔除违反公平性的规则
             └ History[]                     上层决策 + 玩家是否适应
   ▼
④ CHOOSE     IAIProvider::RequestIntentAsync(Context, OnDone) ──> FDirectorIntent
             ├ FLocalProvider   规则表        （永远可用·基线）
             ├ FLlmProvider     HTTP + JSON   （可失败）
             └ FReplayProvider  预录脚本      （确定性·录屏与集成测试）
   ▼
⑤ VALIDATE   四道护栏                                                 【可单测】
             ├ Schema    字段合法、权重和为 1、标签在白名单内
             ├ Budget    Σcost ≤ ChallengeBudget
             ├ Conflict  互斥规则不同时出现（弹药↓ + 远程伤害↓ = 无解）
             └ Fairness  同一规则不连续 3 层；置信度 <0.6 时禁重度调整
             失败 ──> 降级 FLocalProvider，日志记录降级原因
   ▼
⑥ RESOLVE    查表 (Rule.Ammo, medium) ──> ×0.75 ──> FDirectorDecision  【数值在此产生】
   ▼
⑦ APPLY      ├ FloorGenerator   刷怪权重 / 全局规则 / 掉落表
             ├ ReportUI         画像 + 调整项 + 台词
             └ DecisionLog      时间轴，可导出 JSON
```

**LLM 延迟（DeepSeek 实测 3.8–5.0s）被导演报告卡完全吸收**：卡片等玩家读完才开打，
延迟藏在阅读时间里，且不阻塞任何战斗帧。

### 一次真实决策

```text
输入画像：远程攻击占比 90% · 受击极少 · 连续 3 层同一打法（置信度 0.9）
   ▼
约束：第 3 层预算 55 · 弹药规则可用（上层未用过）
   ▼
LLM 选择：Enemy.Tank +0.3（压缩输出空间）· Enemy.Rush +0.2（打断站桩）
          Rule.Ammo / medium
          "你的弓用得很好。但这一层，别指望站在原地。"
   ▼
护栏：预算 20+15+20=55 ✓ · 无互斥冲突 ✓ · 弹药未连续 3 层 ✓
   ▼
映射：Rule.Ammo × medium ──> 弹药掉落 ×0.75
```

---

## 模块边界

单向依赖，同层只经接口/事件通信。**每个模块的"不做"和"做"同样重要。**

| 模块 | 类型 | 做 | 明确不做 |
|---|---|---|---|
| `EventBus` | `UGameInstanceSubsystem` | 广播类型化玩法事件 | 不存状态、不做业务逻辑 |
| `BehaviorRecorder` | `UGameInstanceSubsystem` | 累积层行为快照 | 不分析、不打分、不决策 |
| `ProfileAnalyzer` | **纯静态函数** | 快照 + 历史 → 五维画像 | 不碰 UObject / LLM / 随机数 |
| `DirectorCore` | `UGameInstanceSubsystem` | 编排：约束 → 请求 → 校验 → 映射 | 不直接改游戏对象、不发 HTTP |
| `IAIProvider` | 接口（3 实现） | 在给定约束内产出 Intent | 不做校验、不接触数值 |
| `FloorGenerator` | `UObject` | 消费 Decision 执行 | **不做任何决策** |

`FDirectorDecision` 是 AI 层与玩法层的**唯一**接口——玩法层不知道决策来自 LLM 还是本地表。

### 职责分工

| 职责 | 归属 | 理由 |
|---|---|---|
| 行为 → 五维画像 | C++ 纯函数 | 必须确定、可测、可离线 |
| 预算 / 冲突 / 公平护栏 | C++ | 平衡安全底线，绝不交给 LLM |
| 标签 → 数值映射 | C++ 查表 | LLM 不接触数值 |
| 失败兜底 | C++ | 断网必须可玩 |
| **选哪些规则组合、针对哪个维度** | **LLM** | 多个合法解时做有品味的选择 |
| **台词 / 决策解释** | **LLM** | 规则表写不出的自然语言 |

---

## 当前状态

诚实标注，未完成的部分不假装完成。

| 模块 | 状态 |
|---|---|
| 战斗切片（twin-stick 移动/瞄准、近战、敌人 BT、接触伤害、死亡） | ✅ 已完成（tag `sprint-1-done`） |
| 核心数据契约（`FDirectorDecision` / `FPlayerProfile` / `FFloorBehaviorSnapshot`） | ✅ 已定义 · [`SHMCoreTypes.h`](UnrealProject/Source/ShanHaiMirror/Framework/SHMCoreTypes.h) |
| `EventBus` 类型化事件 + 9 个广播点 | ✅ 已完成 · [`SHMEventBus.h`](UnrealProject/Source/ShanHaiMirror/Framework/SHMEventBus.h) |
| 原生 GameplayTag 注册（编译期标签安全） | ✅ 已完成 · [`SHMGameplayTags.h`](UnrealProject/Source/ShanHaiMirror/Framework/SHMGameplayTags.h) |
| `BehaviorRecorder`（链路 ①） | ✅ 已完成 |
| `ProfileAnalyzer`（链路 ②，五维） | ✅ 已完成 |
| `DirectorCore` + 四道护栏（链路 ③⑤⑥） | ✅ 已完成 · Intent/Decision 类型分离 · 规则表 CSV 查表 |
| 单元测试（画像 + 护栏 + Provider + 遭遇 + 武器 + JSON + 回放 + 日志契约） | ✅ **57/57 全绿** · [`Docs/TestResults.md`](Docs/TestResults.md) |
| 决策链路端到端 | ✅ 控制台 `SHM.DumpDecision` / `SHM.DumpDecisionAsync` |
| 武器切换 + 弓（画像分化的输入源） | ✅ 已完成 · 攻击按 AttackPattern 分发 |
| 敌人四原型 + 遭遇系统消费敌人权重 | ✅ 已完成 · 数据驱动（CSV）· 刷怪点导航网格投影 |
| **闭环端到端可玩** | ✅ 打一局 3 层：真实行为 → 画像 → 决策 → 下层刷怪与规则生效，肉眼可见被针对 |
| `IAIProvider` 三实现（链路 ④） | ✅ Local（降级终点）· **Llm**（OpenAI 兼容，异步）· **Replay**（确定性回放） |
| **三级降级链路** | ✅ Provider 失败 → 护栏拒绝 → 安全兜底，每级留日志；**无 key/断网完整可玩** |
| 决策日志（含护栏前 RawIntent + 溯源） | ✅ 一局结束自动导出 JSON（`schemaVersion` 契约，回放/可视化共用） |
| **导演报告卡（链路 ⑦）** | ✅ 层间弹出「我看到的 → 本层调整 → 白泽台词 → 决策溯源」，读完才开打 |
| **镜界时间轴** | ✅ `SHM.Timeline` 整局回放：想改什么 → 护栏拦没拦 → 实改什么 |
| **AI 导演开/关对照** | ✅ `SHM.Director 0/1`，关闭后退化为固定难度刷怪，用于对照演示 |
| 本局统计（简历数字来源） | ✅ `SHM.Stats`：护栏分道拦截数 · 降级率 · 决策耗时 |
| **Web 决策回放器**（`WebReplay/`，D-21） | ✅ M0–M5 完成并上线 · 护栏前后对照 · 画像雷达图 · 拖拽载入 · Vitest 84 个 · [Live Demo](https://roseri66.github.io/ShanHaiMirror/) |

开发过程记录：[`Docs/Sprint开发总结.md`](Docs/Sprint开发总结.md)（六次开工复盘，含设计判断、计划偏离与修复教训）· [`Docs/踩坑记录.md`](Docs/踩坑记录.md)（25 条，每条含现象/原因/解法/规则）

> **实测记录（DeepSeek `deepseek-chat`，OpenAI 兼容端点）**：单次决策往返 3.8–5.0s。
> 三次真实调用分别走通了三条路径——① LLM 同时选中互斥规则（弹药↓ + 远程伤害↓，
> 对远程玩家是无解组合）被 **Conflict 护栏拒绝并降级**；② 超时降级；③ 直采通过，
> 台词「箭矢不够用的时候，你还能保持从容吗？」。第 ① 条是"护栏确实在约束 LLM"的实测证据。

**范围与每一条取舍的理由见 [`Docs/DECISIONS.md`](Docs/DECISIONS.md)**，包含 21 条决策记录（砍掉什么、为什么砍、代价是什么、以及时间不够时的削减顺序）。MVP 冻结后的每一条扩展都要在 §7 先追一条显式决策——**不追决策就动手，等于自己破自己的规矩**。

---

## 最快的判断方式：[打开决策回放器](https://roseri66.github.io/ShanHaiMirror/)

一屏静态页面，把一份决策日志渲染成「LLM 想改什么 → 四道护栏拦没拦 → 实际改了什么」。
**左表没有「倍率」列，右表才有** —— "数值只在护栏之后产生"这条架构主张，
在页面上是两张表的列数差，不是一句自我宣称。

源码在 [`WebReplay/`](WebReplay/)：Vue 3 + TypeScript，不引 UI 框架、状态管理库和图表库
（雷达图手写 SVG）。TS 类型是 [`SHMDecisionLogFormat.h`](UnrealProject/Source/ShanHaiMirror/Director/SHMDecisionLogFormat.h)
的镜像 —— 日志格式是跨语言契约，靠类型挡住字段漂移。

---

## 想在引擎里看？跑这四条命令

进 PIE 后打开控制台（`` ` ``）：

| 命令 | 看什么 |
|---|---|
| `SHM.DumpDecisionAsync ranger 2` | 走真实 Provider 跑一次决策，打印「出自谁 · 耗时 · 护栏拦截几项」 |
| `SHM.Timeline` | **整局回放**：每层「想改什么（护栏前）→ 护栏拦没拦 → 实改什么（带数值）」 |
| `SHM.Director 0` | 关掉 AI 导演，再打一局 —— 三层配比一个样，白泽沉默。**对照才能证明针对是真的** |
| `SHM.Stats` | 本局统计：护栏分道拦截数 · 降级率 · 决策平均耗时 |

不想跑项目，直接读[**三份样例日志**](Docs/samples/)也够——它们是同一套链路的三个典型场景，
**都是引擎真实跑出来的，没有一份是手写 JSON**：

| 样例 | 性质 | 证明什么 |
|---|---|---|
| [`DecisionLog_Sample.json`](Docs/samples/DecisionLog_Sample.json) | 真实对局 · LLM 直采 | 顺利路径跑得通，LLM 确实承担"选择与表达" |
| [`DecisionLog_Guardrail_Run.json`](Docs/samples/DecisionLog_Guardrail_Run.json) | 真实对局 · **护栏拦下 2 次** | 护栏在真实对局中确实拦截并降级，游戏不中断；画像随层演进（置信度 0.50→0.70），等级递进 Stable→Pressure→Counter |
| [`DecisionLog_Guardrail_Ideal.json`](Docs/samples/DecisionLog_Guardrail_Ideal.json) | **控制台夹具 · 非对局** | 三道护栏结构上都拦得住（刻意构造的理想情况，文件顶部已标注全部限制） |

> 每份文件顶部都有 `_kind` / `_note` 字段说明它是怎么来的、有什么限制。
> 第三份**不是一局游戏**：第 4 层在真实对局里到不了（`TotalFloors = 3`），
> 画像是控制台写死的常量。它存在的意义只是证明三道护栏各自拦得住——
> 真实对局里的护栏拦截看第二份。详见 [`Docs/samples/README.md`](Docs/samples/README.md)。

加上上面那段 DeepSeek 实测记录，就是这个项目全部的"护栏在约束 LLM"的证据。

---

## 代码导览

想快速判断这个项目的，按这个顺序看：

| 看什么 | 在哪 |
|---|---|
| **数据契约**（最能说明设计意图） | `Source/ShanHaiMirror/Framework/SHMCoreTypes.h` · `Director/SHMDirectorTypes.h` |
| **Intent/Decision 分离 + 四道护栏**（本项目核心主张的落点） | `Director/SHMDirectorTypes.h` · `Director/SHMDecisionValidator.h` |
| 决策编排（③→⑥ 串联、观察层短路、安全兜底） | `Director/SHMDirectorCore.cpp` |
| 单元测试（TDD 全程，53 用例） | `Source/ShanHaiMirror/Tests/` |
| **决策日志格式契约**（字段名与取值域的唯一真源，三方共用） | `Director/SHMDecisionLogFormat.h` |
| Web 回放器的 TS 契约镜像 + 不信任 JSON 的解析器 | `WebReplay/src/types/` |
| 范围决策与理由 | `Docs/DECISIONS.md` |
| 分层架构与 AI Director 详细设计 | `Docs/oldDocs/TDD.md` §1、§3 |
| 事件总线 | `Source/ShanHaiMirror/Framework/SHMEventBus.h` |
| 战斗组件 | `Source/ShanHaiMirror/Framework/SHM*Component.h` |
| 敌人 AI（个体行为树，**与 AI 导演无关**） | `Source/ShanHaiMirror/Enemies/` |

> 命名澄清：**敌人个体 AI** = 行为树（单只怪怎么追怎么打）；**AI 导演** = Director 子系统（整层怎么配、改什么规则）。两者完全不同。

---

## 构建

需要 Unreal Engine 5.5 + MSVC。

```
UnrealProject/ShanHaiMirror.uproject     右键 Generate Visual Studio project files
UnrealProject/ShanHaiMirror.sln          Development Editor | Win64
```

LLM Provider 走**任一 OpenAI 兼容端点**，全部通过环境变量配置（**未配置时自动使用本地
Provider，游戏完整可玩**）：

```powershell
setx SHM_LLM_API_KEY  "sk-..."                      # 必填，缺省则用本地 Provider
setx SHM_LLM_BASE_URL "https://api.deepseek.com/v1" # 选填，默认 OpenAI
setx SHM_LLM_MODEL    "deepseek-chat"               # 选填
setx SHM_LLM_TIMEOUT  "10"                          # 选填，秒
```

设置后需**重启编辑器**（环境变量在进程启动时读取）。key 只存在于内存，
不入库、不进日志。删除用 `[Environment]::SetEnvironmentVariable("SHM_LLM_API_KEY", $null, "User")`
——`setx` 无法置空（踩坑 #20）。

---

## 一些设计选择的理由

**不使用 GAS。** GameplayEffect 确实天然适合规则修改器，但 ASC 复制、Attribute Set、GE 执行链这些复杂度是为多人联机设计的，单机俯视角完全用不到。改为自研 `USHMAttributeComponent`（Base/Flat/Pct 三段）+ 全量 GameplayTags。规则系统需要的"确定性、可测试、断网可跑"恰恰是自研最容易保证、GAS 最难保证的。详见 [`Docs/oldDocs/TDD.md`](Docs/oldDocs/TDD.md) §5.2。

**LLM 只在层间调用一次，不在战斗中调用。** 动作游戏里没人等 LLM。层间的一次性调用足够，多轮 agent 编排是自找麻烦。

**武器和敌人都带 GameplayTag，AI 读 Tag 不读名字。** 这是整个画像系统能工作的地基——加一种新武器不需要改任何 AI 代码。

**`FReplayProvider` 值得单独提一句。** 它读预录 JSON 脚本产出决策，成本极低，但同时解决了两个问题：录屏时决策确定性可复现，集成测试时不依赖网络。一个抽象吃掉三个不同需求，这是保留 `IAIProvider` 而不是直接写 HTTP 调用的理由。

---

## 许可

本仓库的代码与文档采用 **MIT 许可证**，见 [`LICENSE`](LICENSE)。

覆盖范围是本仓库自有的内容：`UnrealProject/Source/`（C++）、`WebReplay/`（前端）、
`UnrealProject/Data/`（CSV 数据表）、`UnrealProject/Content/` 下的蓝图与 `Docs/`。
不含任何第三方素材——本项目不使用美术资源，视觉全部是引擎 primitive + 纯色材质。

**Unreal Engine 本身不在此列**，它归 Epic Games 所有并受 [Unreal Engine EULA](https://www.unrealengine.com/eula) 约束。
构建本项目需要你自行安装 UE 5.5 并接受该协议。
