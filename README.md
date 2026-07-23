# 山海镜 ShanHaiMirror

**一个 AI 导演系统的架构实现。** UE 5.5 · C++ · 单人开发。

这个仓库要展示的不是一个游戏，是一套**在游戏运行时约束 LLM 做决策**的架构：LLM 负责"选择与表达"，代码负责"计算与安全"，两者的边界由类型系统而非纪律来保证。断网时系统完整降级，玩家无感知。

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
④ CHOOSE     IAIProvider::RequestIntent(Context) ──> FDirectorIntent
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

**LLM 的 1-2 秒延迟被"导演报告"界面自然掩盖，不阻塞任何战斗帧。**

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
| `BehaviorRecorder` | `UWorldSubsystem` | 累积层行为快照 | 不分析、不打分、不决策 |
| `ProfileAnalyzer` | **纯静态函数** | 快照 + 历史 → 五维画像 | 不碰 UObject / LLM / 随机数 |
| `DirectorCore` | `UWorldSubsystem` | 编排：约束 → 请求 → 校验 → 映射 | 不直接改游戏对象、不发 HTTP |
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
| `IAIProvider`（链路 ④） | 🟡 `FSHMLocalProvider` 已完成（降级终点，单独即完整导演）；Llm / Replay 两实现计划中 |
| 单元测试（画像 + 护栏 + Provider） | ✅ **30/30 全绿** · [`Docs/TestResults.md`](Docs/TestResults.md) |
| 决策链路端到端 | ✅ 控制台 `SHM.DumpDecision` 手喂画像打印完整决策（headless 已验证） |
| 属性 / 能力 / 武器 / Build 组件 | 🟡 骨架已有 |
| 武器切换 + 弓（画像分化的输入源） | ⬜ 未开始（下一阶段） |
| 遭遇系统消费敌人权重（决策接玩法） | ⬜ 未开始（下一阶段，闭环肉眼可见的节点） |
| 决策日志 + 导演报告 UI（链路 ⑦） | ⬜ 未开始 |

开发过程记录：[`Docs/Sprint开发总结.md`](Docs/Sprint开发总结.md)（两个 Sprint 的复盘，含设计判断与修复教训）· [`Docs/踩坑记录.md`](Docs/踩坑记录.md)（15 条，每条含现象/原因/解法/规则）

**范围与每一条取舍的理由见 [`Docs/DECISIONS.md`](Docs/DECISIONS.md)**，包含 20 条决策记录（砍掉什么、为什么砍、代价是什么、以及时间不够时的削减顺序）。

---

## 代码导览

想快速判断这个项目的，按这个顺序看：

| 看什么 | 在哪 |
|---|---|
| **数据契约**（最能说明设计意图） | `Source/ShanHaiMirror/Framework/SHMCoreTypes.h` · `Director/SHMDirectorTypes.h` |
| **Intent/Decision 分离 + 四道护栏**（本项目核心主张的落点） | `Director/SHMDirectorTypes.h` · `Director/SHMDecisionValidator.h` |
| 决策编排（③→⑥ 串联、观察层短路、安全兜底） | `Director/SHMDirectorCore.cpp` |
| 单元测试（TDD 全程，30 用例） | `Source/ShanHaiMirror/Tests/` |
| 范围决策与理由 | `Docs/DECISIONS.md` |
| 分层架构与 AI Director 详细设计 | `Docs/TDD.md` §1、§3 |
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

LLM Provider 需要一个 OpenAI 兼容端点，通过环境变量配置（未配置时自动使用 `FLocalProvider`，游戏完整可玩）。

---

## 一些设计选择的理由

**不使用 GAS。** GameplayEffect 确实天然适合规则修改器，但 ASC 复制、Attribute Set、GE 执行链这些复杂度是为多人联机设计的，单机俯视角完全用不到。改为自研 `USHMAttributeComponent`（Base/Flat/Pct 三段）+ 全量 GameplayTags。规则系统需要的"确定性、可测试、断网可跑"恰恰是自研最容易保证、GAS 最难保证的。详见 [`Docs/TDD.md`](Docs/TDD.md) §5.2。

**LLM 只在层间调用一次，不在战斗中调用。** 动作游戏里没人等 LLM。层间的一次性调用足够，多轮 agent 编排是自找麻烦。

**武器和敌人都带 GameplayTag，AI 读 Tag 不读名字。** 这是整个画像系统能工作的地基——加一种新武器不需要改任何 AI 代码。

**`FReplayProvider` 值得单独提一句。** 它读预录 JSON 脚本产出决策，成本极低，但同时解决了两个问题：录屏时决策确定性可复现，集成测试时不依赖网络。一个抽象吃掉三个不同需求，这是保留 `IAIProvider` 而不是直接写 HTTP 调用的理由。
