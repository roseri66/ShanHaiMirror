# 《山海镜》范围决策记录（DECISIONS.md）

> **版本**：v1.0 · 2026-07-21
> **状态**：本文件是范围的**唯一权威**。GDD / TDD / Spec / Design Bible 保留为设计存档，凡与本文件冲突处，以本文件为准。
> **约束**：单人开发 · 无美术资源 · 8-10 小时/周 · 4-6 周后冻结（总预算 ≈ 32-60 小时，按 45 小时规划）

---

## 0. 定位变更

| | 旧定位 | 新定位 |
|---|---|---|
| 目标 | 交付一个可玩 30 分钟的完整 Demo，参加腾讯游戏开发大赛 | **求职作品集第二项目**，证明**架构设计能力**与 **AI 系统设计能力** |
| 受众 | 玩家 / 比赛评委 | **面试官 / 技术评审**，平均停留 10 分钟：读 README → 翻代码 → 看一段录屏 |
| 成功判据 | 完整跑通 6 层、时长达标、视觉成品度 | **AI 导演决策链路可读、可测、可复现**；有人能在 10 分钟内讲清它怎么工作 |
| 失败模式 | 内容不够多 | **摊薄**——把 45 小时铺在角色/Boss/关卡/美术上，每项都半成品，架构主张淹没在残缺内容里 |

**一句话原则：新受众不玩游戏，他们读系统。凡是只在"玩家时长"里兑现价值的功能，一律 out of scope。**

---

## 1. 决策总表

| # | 项目 | 决策 | 一句话理由 |
|---|---|---|---|
| D-01 | 6 层 / 30 分钟单局 | ❌ OUT | 时长是内容问题，对架构证据零贡献 |
| D-02 | 3 层短循环 | ✅ IN | 展示完整闭环所需的**最小**层数 |
| D-03 | 第 2 角色（方士） | ❌ OUT | 用武器切换实现近战/远程画像分化，更省且演示效果相同 |
| D-04 | 烛龙 Boss | ❌ OUT | 单点内容，成本高、复用为零 |
| D-05 | 敌人 10-15 种 | ❌ OUT（保留 4 原型） | AI 权重的作用面是**原型**，变体纯属外观 |
| D-06 | 全部美术管线 | ❌ OUT | 无美术资源是硬约束；视觉不在评估范围 |
| D-07 | 比赛相关全部事项 | ❌ OUT | 不再参赛 |
| D-08 | GameNPC 通道 | ❌ OUT（换 OpenAI 兼容） | 依赖参赛资格；`IAIProvider` 抽象本身才是架构证据 |
| D-09 | 强化三选一 Pick3 | ❌ OUT | 第 4 个作用面，边际证明力低于其成本 |
| D-10 | 节点图关卡 / 房间模板库 | ❌ OUT（线性 3 房间） | 对 AI 卖点零贡献 |
| D-11 | 战场偏好 arena_preference | ❌ OUT（字段保留待实现） | 需要多套房间几何，被 D-06/D-10 连带砍掉 |
| D-12 | 跨局记忆 | ❌ OUT | 依赖存档 + 多局游玩，无玩家可依赖 |
| D-13 | 温和/整人人格、元素、商店、进化 | ❌ OUT（维持原判） | 早已在 GDD v0.2 出局，此处确认不复活 |
| D-14 | AI 导演核心闭环 | ✅ IN ★ | **唯一深化方向** |
| D-15 | Intent / Decision 分离 | ✅ IN ★ | 让"LLM 不碰数值"从口号变成类型系统强制 |
| D-16 | 三 Provider（Local / LLM / Replay） | ✅ IN ★ | 降级、可测、可复现录屏，一个抽象吃三个需求 |
| D-17 | 决策日志（镜界时间轴降级版） | ✅ IN | 纯代码，是"AI 在思考"最强的可见证据 |
| D-18 | Director 开/关对照开关 | ✅ IN | 成本 ≈ 一个 bool，演示价值最高 |
| D-19 | 单元测试（画像 + 护栏） | ✅ IN | TDD §3.2 已承诺"可单测"，现在把它变成验收条件 |
| D-20 | 武器切换 + 弓 | ✅ IN | 玩家应对 AI 针对的**唯一**手段，闭环不能断 |

---

## 2. 砍掉的项（OUT OF SCOPE）

以下内容**保留在设计文档中作为存档**，但不进入 4-6 周实现计划。原文档不删除、不改写，仅在顶部加 scope banner 指回本文件。

### D-01 · 6 层 / 30 分钟单局 → 砍
- **原设计**：GDD §8.2、TDD §2.7 的 6 层结构；单人版保底 5 层 ~24min + 跨局记忆。
- **理由**：时长指标服务的是"玩家体验密度"，新受众根本不会玩到第 3 层。30 分钟的成本几乎全落在内容和平衡调参上（TDD §2.7 自己承认了这一点），而这两项产出的都不是架构证据。
- **代价**：失去"内容放大器"这个论证。**接受**——面试官不问这个。

### D-03 · 第二角色（方士） → 砍
- **原设计**：GDD §3.3、Spec S4-F2。
- **理由**：角色数量证明不了任何架构能力。而 AI 导演演示需要的"远程玩家 vs 近战玩家画像分化"，用**同一角色的武器切换**（剑 / 弓，D-20）即可完全实现，成本不到新角色的三分之一。
- **代价**：无。这是纯粹的成本削减，演示效果不变。

### D-04 · 烛龙 Boss → 砍
- **原设计**：GDD §10、Spec S4-F3。
- **理由**：Boss 是一次性内容，三阶段 + 动态技能池的实现成本约等于整个导演护栏层。它证明的是"能做 Boss 战"，不是本项目要证明的东西。
- **代价**：单局没有终点仪式感。用第 3 层结束后的**决策时间轴回放**作为结算屏替代（D-17）。

### D-05 · 敌人补至 10-15 种 → 砍，保留 4 原型
- **原设计**：TDD §2.5 的 7 原型 + 10-15 具体异兽。
- **理由**：AI Director 只输出**原型权重**（这是原设计里正确的一条），具体异兽是原型的皮肤。既然没有美术（D-06），变体只会是同一个胶囊体换颜色——纯粹的空转。
- **保留**：Grunt / Tank / Rush / Shooter 四原型。这四个已经能构成完整的克制关系矩阵（远程玩家 → Tank+Rush 施压，近战玩家 → Shooter+Controller 施压）。Controller / Support / Elite 出局。
- **代价**：克制矩阵略窄。**接受**——4 个原型足够让一次定向反制被肉眼看见。

### D-06 · 全部美术管线 → 砍
- **原设计**：GDD §12.6、TDD §0/§7.4/§7.5（AI 生成 2D/3D、技术美术、镜界视觉、Niagara、资产台账、三条法律护栏、Git LFS）。
- **理由**：约束直接写死"无美术资源"。原方案的 AI 美术管线本身就是一条 20+ 小时的独立工作流，在 45 小时总预算里不可能存在。
- **替代**：引擎 primitive + 纯色材质，按原型区分颜色（Tank 蓝 / Rush 红 / Shooter 黄 / Grunt 灰）。**在 README 里显式声明"程序员美术，视觉不在本项目评估范围"**——主动声明比让人误以为你尽力了要好。
- **连带砍掉**：资产台账、知识产权合规条款、Git LFS 配置、Trailer/录制规格、镜界风格基线。

### D-07 / D-08 · 比赛事项 + GameNPC 通道 → 砍
- **原设计**：TDD §7.5 合规、§8 评委视角审查、Spec S5-F1 的 GameNPC 鉴权预研。
- **理由**：不再参赛。GameNPC 依赖参赛资格且鉴权/QPS 全是未知风险，把它放在 45 小时的关键路径上是不可接受的。
- **替代**：`IAIProvider` 保留（它本来就是抽象层），正式实现换成任一 **OpenAI 兼容 HTTP 端点**，key 走环境变量。**接口抽象本身就是要展示的架构能力**——换 provider 只改一个类，这正好是证据。
- **保留价值**：TDD §8.3「最容易被评委质疑的地方」那张表**不要删**，把它改称"设计自审"——它展示的是自我批判能力，对面试官同样有效。

### D-09 · 强化三选一（Pick 3） → 砍
- **原设计**：GDD §7、TDD §2.6、Spec S4-F1（~15 个被动 + 叠加系统 + 选择 UI + AI 权重诱导）。
- **理由**：这是 AI 导演的**第 4 个作用面**。前三个（敌人权重 / 规则修改 / 资源分布）已经足以证明决策链路完整；第 4 个的边际证明力很低，但要付出被动数据表 + 叠加逻辑 + 一套 UI 的成本。
- **代价**：玩家应对 AI 针对的手段少一个。**由 D-20 武器切换承担**——闭环不断。
- **注意**：这是本次唯一有争议的削减。若实施到第 4 周仍有余量，它是**第一个**该被放回来的项。

### D-10 / D-11 · 节点图关卡、房间模板库、战场偏好 → 砍
- **原设计**：TDD §2.7（Slay the Spire 式节点图、6-8 房间模板、arena_preference）。
- **理由**：自由探索的节点图对"AI 动态调整"零贡献，TDD 自己也把几何生成列为死亡陷阱。战场偏好需要多套房间几何，在 D-06 之后无法实现。
- **替代**：**线性 3 房间/层，单一房间几何复用**。
- **接口处理**：`FDirectorDecision::ArenaPreference` 字段**保留但不消费**，加注释 `// Reserved: 需要多套房间几何，见 DECISIONS.md D-11`。保留字段是为了让决策结构体的设计意图完整，注释是为了不让人以为你漏实现了。

### D-12 · 简版跨局记忆 → 砍
- **原设计**：TDD §6，单人版曾把它从加分项升为"核心储备"。
- **理由**：它的价值全部兑现在"玩家打第二局"这个场景里。作品集没有第二局。
- **代价**：失去"跨 Run 学习"这个最亮的 AI Native 卖点。**接受**——它的前置依赖（存档三层结构 + 多局数据）成本过高。

### D-13 · 温和/整人人格、元素系统、商店、角色进化、白泽配音 → 维持出局

---

## 3. 保留的项（IN SCOPE）

只有四类东西留下来：**AI 导演闭环本身**、**让闭环可见的展示层**、**让闭环可信的测试**、**闭环运转所必需的最小玩法**。

| 项目 | 范围 | 为什么它活下来 |
|---|---|---|
| AI 导演六模块 | §4 全部 | 唯一深化方向 |
| 战斗切片 | 已完成（`sprint-1-done`） | 已入库，零新增成本 |
| 武器切换 + 弓（D-20） | 剑 / 弓两把，Tag 驱动 | 画像分化的输入源 + 玩家应对手段，闭环两端都靠它 |
| 4 敌人原型 + 共享 BT | Grunt/Tank/Rush/Shooter | AI 敌人权重的作用面 |
| 遭遇系统 | 波次刷怪 + Threat Budget | AI 权重落地为实际怪物的执行层 |
| 3 层线性流程 | 每层 3 房间，~8 分钟一局 | 承载 F1 观察 → F2 轻度 → F3 反制的递进 |
| 导演报告 UI | 极简：画像数值 + 本层调整项 + 白泽台词 | 让"AI 在针对我"这件事可见 |
| 决策日志（D-17） | 结构化记录 + 局末回放 + JSON 导出 | 最强的"AI 在思考"证据，纯代码 |
| 开/关对照开关（D-18） | 一个控制台变量 | 成本最低、说服力最高 |
| 单元测试（D-19） | 画像分析器 + 护栏层 | 架构能力最硬的证据 |

**三层的剧本**（决定了为什么是 3 而不是 5）：

| 层 | 导演行为 | 观众看到什么 |
|---|---|---|
| F1 | 只观察，不调整（`Confidence` 未达阈值） | 建立基线 |
| F2 | 首次轻度调整 + 首张导演报告 | "它在看着我" |
| F3 | 高置信度 → **定向反制** | "它在针对我的打法" |

三层是能演出"观察 → 试探 → 反制"递进的最小层数。少一层，AI 就退化成一次性反应，看不出学习过程。

---

## 4. AI 导演系统（唯一深化方向）

### 4.1 模块边界

六个模块，单向依赖，同层只经接口/事件通信。**每个模块的"不做"和"做"同样重要**——边界清晰本身就是要展示的能力。

| 模块 | 类型 | 做 | **明确不做** |
|---|---|---|---|
| **EventBus** | `UGameInstanceSubsystem` | 广播类型化玩法事件 | 不存状态、不做业务逻辑 |
| **BehaviorRecorder** | `UWorldSubsystem` | 订阅事件，累积 `FFloorBehaviorSnapshot` | 不分析、不打分、不决策 |
| **ProfileAnalyzer** | **纯静态函数** | 快照 + 历史 → `FPlayerProfile` | **不碰 UObject、不碰 LLM、不碰随机数** |
| **DirectorCore** | `UWorldSubsystem` | 编排决策：造约束 → 请求 → 校验 → 映射 | 不直接改游戏对象、不发 HTTP |
| **IAIProvider** | 接口（3 实现） | 在给定约束内产出 `FDirectorIntent` | **不做校验、不接触具体数值** |
| **FloorGenerator** | `UObject` | 消费 `FDirectorDecision` 执行 | **不做任何决策** |

关键约束两条：
1. **`ProfileAnalyzer` 是纯函数**——无副作用、无引擎依赖、输入相同则输出必定相同。这是它能被单测的前提，也是"断网可跑"的前提。
2. **`FDirectorDecision` 是 AI 层与玩法层的唯一接口**——玩法层不知道决策来自 LLM 还是本地表。

### 4.2 两个结构体的分离（D-15，本次核心设计改动）

现有 `SHMCoreTypes.h` 里 `FDirectorDecision` 一个结构体承担了"LLM 的输出"和"玩法层的输入"两个角色。拆开：

```cpp
// FDirectorIntent —— Provider 的输出。只有标签和等级，没有任何数值。
USTRUCT() struct FDirectorIntent
{
    EChallengeLevel        ChallengeLevel;
    TMap<FGameplayTag,float> EnemyWeights;   // Enemy.Tank -> 0.3
    TArray<FRuleIntent>    RuleIntents;      // { Rule.Ammo, "medium" } —— 无数值
    FString                Narration;        // 白泽台词
    FString                Reason;           // 决策理由，进日志
};

// FDirectorDecision —— 玩法层的输入。已查表、已护栏、带具体数值。
// 只能由 DirectorCore 产出，Provider 无法构造。
USTRUCT() struct FDirectorDecision
{
    EChallengeLevel        ChallengeLevel;
    TMap<FGameplayTag,float> EnemyWeights;
    TArray<FRuleModifier>  RuleModifiers;    // 已含 Multiplier 具体数值
    TMap<FGameplayTag,float> ResourceAdjust;
    FGameplayTag           ArenaPreference;  // Reserved, 见 D-11
    FString                Narration;
    FDirectorTrace         Trace;            // 决策溯源，进日志
};
```

**为什么这个分离值得做**：TDD §3.4 早就主张"LLM 输出里没有任何具体数值"，但当时它只是一条纪律。拆成两个类型之后，这条纪律由**类型系统强制**——Provider 在编译期就构造不出带数值的决策。这是"用架构而非约定来保证不变量"的一个具体例证，也是本项目最值得写进 README 的一段。

### 4.3 决策链路

一层游戏的完整生命周期，七步：

```text
战斗中  Combat ──event──> EventBus ──> BehaviorRecorder（实时累积）
   │
   ▼ OnFloorFinished
① OBSERVE    BehaviorRecorder 定稿 ──> FFloorBehaviorSnapshot
   ▼
② ANALYZE    ProfileAnalyzer(Snapshot, History) ──> FPlayerProfile   【纯函数·可单测】
   ▼
③ CONSTRAIN  DirectorCore 构建 FDirectorContext                      【约束在此收敛】
             ├ ChallengeBudget(FloorIndex)      预算上限
             ├ AvailableRules[]                 已剔除违反公平性的规则
             ├ AvailableArchetypes[]            4 原型
             └ History[]                        上层决策 + 玩家是否适应
   ▼
④ CHOOSE     IAIProvider::RequestIntent(Context) ──> FDirectorIntent
             ├ FLocalProvider   规则表查表        （永远可用·基线）
             ├ FLlmProvider     HTTP + JSON       （可失败）
             └ FReplayProvider  读脚本 JSON       （确定性·录屏/测试）
   ▼
⑤ VALIDATE   DecisionValidator 四道护栏                              【可单测】
             ├ Schema      字段合法、权重和为 1、标签在白名单内
             ├ Budget      Σcost ≤ ChallengeBudget
             ├ Conflict    互斥规则不同时出现（如 弹药↓ + 远程伤害↓ = 无解）
             └ Fairness    同一规则不连续 3 层出现；Confidence<0.6 时禁重度
             失败 ──> 降级到 FLocalProvider，日志记录降级原因
   ▼
⑥ RESOLVE    RuleResolver 查 DT_Rule：(Rule.Ammo, medium) ──> ×0.75
             ──> FDirectorDecision                                   【数值只在这一步产生】
   ▼
⑦ APPLY      ├ FloorGenerator.BuildFloor()   刷怪权重 / 全局规则 / 掉落表
             ├ DirectorReportUI              画像 + 调整项 + 白泽台词
             └ DecisionLog.Record()          进时间轴，可导出 JSON
```

**四个不变量**（README 里应当逐条声明，它们是本项目的技术主张）：

1. **数值只在第 ⑥ 步产生。** LLM 永远看不到也改不了任何数值——它只在标签空间里选择。
2. **第 ④ 步可以整体失败。** 任何一层失败都降级到本地表，玩家无感知，日志留痕。**拔掉网线，游戏照常运转。**
3. **第 ② 步是纯函数。** 同样的输入必定得到同样的画像，可单测、可离线复现。
4. **约束在第 ③ 步收敛，校验在第 ⑤ 步兜底。** LLM 拿到的候选集已经是安全的；即便如此仍然全量校验——**不信任 LLM 输出**是设计前提，不是补丁。

### 4.4 输入 / 输出契约

**Provider 输入**（`FDirectorContext` 序列化）：

```json
{
  "profile": {
    "build_concentration": 90, "combat_efficiency": 88,
    "resource_surplus": 70, "strategy_switch": 12, "survival_pressure": 10,
    "archetype": "Ranger", "primary_build": ["Build.Ranged"], "confidence": 0.9
  },
  "context": { "floor": 3, "total_floors": 3, "challenge_budget": 55 },
  "history": [ { "floor": 2, "rules": ["Rule.Ammo"], "player_adapted": false } ],
  "available_rules": [
    { "tag": "Rule.Ammo",     "cost": 20, "level": "medium" },
    { "tag": "Rule.Cooldown", "cost": 15, "level": "light"  }
  ],
  "available_archetypes": ["Enemy.Grunt","Enemy.Tank","Enemy.Rush","Enemy.Shooter"]
}
```

**Provider 输出**（`FDirectorIntent`，严格 schema，**无数值**）：

```json
{
  "challenge_level": 2,
  "enemy_weights": { "Enemy.Tank": 0.3, "Enemy.Rush": 0.2, "Enemy.Grunt": 0.5 },
  "rule_intents": [ { "tag": "Rule.Ammo", "level": "medium" } ],
  "narration": "你的弓用得很好。但这一层，别指望站在原地。",
  "reason": "连续三层远程站桩，高置信度，压缩输出空间并施加移动压力。"
}
```

**分工**（保留 TDD §3.5 的判断，未变）：

| 职责 | 归属 | 理由 |
|---|---|---|
| 行为 → 五维画像 | **C++ 纯函数** | 必须确定、可测、可离线 |
| 预算 / 冲突 / 公平护栏 | **C++** | 平衡安全底线，绝不交给 LLM |
| 标签 → 数值映射 | **C++ 查表** | LLM 不接触数值 |
| 失败兜底 | **C++** | 断网必须可玩 |
| **选哪些规则组合、针对哪个维度** | **LLM** | 多个合法解时做有品味的选择 |
| **白泽台词 / 决策解释** | **LLM** | 规则表写不出的自然语言 |

### 4.5 三个 Provider（D-16）

一个抽象吃掉三个不同需求，这是保留它的理由：

| 实现 | 用途 | 为什么必需 |
|---|---|---|
| `FLocalProvider` | 规则表决策 | 降级终点。**它单独就是一个完整可玩的游戏**——LLM 是可拔插增强层，不是依赖 |
| `FLlmProvider` | OpenAI 兼容 HTTP | 证明"选择与表达"确实由 LLM 承担 |
| `FReplayProvider` | 读预录 JSON 脚本 | 录屏确定性 + 集成测试可复现。成本极低，价值极高 |

### 4.6 验收标准

冻结时必须全部为真，否则不算完成：

- [ ] 一局 3 层跑通，F1 观察 / F2 轻度 / F3 定向反制三种行为肉眼可辨
- [ ] 拔掉网络（或强制 `FLlmProvider` 失败），游戏完整可玩，日志显示降级原因
- [ ] `ProfileAnalyzer` 单测覆盖五个维度的边界情况（零攻击、单一武器 100%、均匀分布）
- [ ] `DecisionValidator` 单测覆盖四道护栏各自的拒绝路径
- [ ] 决策日志可导出 JSON，包含每层的 输入画像 → Intent → 护栏结果 → 最终 Decision
- [ ] 开/关 Director 对照可在同一存档下切换
- [ ] README 能让一个没读过代码的人在 10 分钟内讲清决策链路

### 4.7 削减优先级（时间不够时按序砍）

预算超支时**从上往下砍，不允许跳序**：

1. 第 3 层 → 退到 2 层（保留观察 → 反制，牺牲递进感）
2. 敌人原型 4 → 3（去掉 Shooter）
3. 导演报告 UI 精修 → 纯文本框
4. 决策日志回放 UI → 只导出 JSON + 控制台打印

**红线：以下四项在任何情况下不砍** —— `IAIProvider` 三实现、护栏四道、`ProfileAnalyzer` 单测、开/关对照开关。砍掉其中任何一项，这个项目就失去了它存在的理由。

---

## 5. 对既有文档的处理

不删除任何文档。四份存档文件顶部加统一 banner：

> ⚠️ **本文档为设计存档（2026-07-21 之前）。** 项目已重新定位为作品集项目，实际范围以 [`DECISIONS.md`](./DECISIONS.md) 为准。本文中大量内容已标记为 out of scope。

| 文档 | 处理 |
|---|---|
| `GDD.md` | 加 banner。§8 关卡、§10 Boss、§7 强化、§12.6 美术标 OUT |
| `TDD.md` | 加 banner。**§1 分层架构、§3 AI Director、§5.2 GAS 选型仍然有效**，其余标 OUT |
| `Spec.md` | 加 banner。S1-S3 有效，S4-S6 标 OUT |
| `Design Bible v1.0.md` | 加 banner，整体标为「愿景存档，不实现」 |
| `README.md` | **重写**——这是面试官的第一落点，必须讲架构而不是讲游戏 |

---

## 6. 本次未做的事

按要求，本文件只做减法。以下内容**刻意没有提出**：新功能、新系统、任何形式的范围扩展。§4.2 的 Intent/Decision 分离与 §4.5 的 Replay Provider 是既有设计的**收紧与澄清**（把 TDD 已承诺的纪律落到类型系统上），不是新增能力。
