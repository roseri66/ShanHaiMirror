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
多个 Provider 实现同一接口，失败逐级降级到本地规则表，日志留痕。**本地 Provider 单独就是一个完整可玩的游戏**——LLM 是可拔插的增强层，不是依赖。拔掉网线、或后端服务停掉，游戏都照常运转。

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
             ├ FRemoteProvider  HTTP → 决策网关（可失败·生产路径）
             ├ FLocalProvider   规则表        （永远可用·降级终点）
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
| `IAIProvider` | 接口（4 实现） | 在给定约束内产出 Intent | 不做校验、不接触数值 |
| `FloorGenerator` | `UObject` | 消费 Decision 执行 | **不做任何决策** |
| `DirectorService` | Spring Boot 服务 | 持有 key、拼 prompt、调 LLM、缓存 | **不做校验**——护栏留在客户端 |

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

## 决策网关 `DirectorService/`（Java 17 + Spring Boot 3）

**存在的理由只有两条**：① key 不能进客户端；② 决策数据要能跨局聚合。
不服务于这两条的功能一律不做——没有账号、没有排行榜、没有管理后台、不拆微服务。

> **两条的兑现状态**（照实写）：① 已完成（第七次开工）；
> ② **聚合能力已具备且已用真实数据跑过**（第八次开工，落库 + 模拟器，见下）——
> 16 局 / 32 条真实流水回流，**推翻了我原本的假设，并挖出修掉一个分桶缺陷**。
> **但桶宽 20 / 候选 3 这两个数字本身仍未校准**：数据说它们不是主要矛盾，
> 真正的切碎源是另一个字段，而改那个字段需要先按新分桶算法重采一批数据。

```
客户端 FSHMRemoteProvider ──HTTP──> DirectorService ──> LLM
   ↑                                      │
   └── 四道护栏在这一侧，不在服务端 ────────┘
```

### 最重要的一条是**否决**：护栏不上服务端

把校验搬到服务端是最容易想到、也最该拒绝的做法。三条理由按硬度排序：

1. **会破坏「断网可玩」这条不变量**——护栏在服务端，则后端不可达时无人校验本地 Provider 的输出
2. **逻辑双写必然漂移**——C++ 一份、Java 一份，两边对 Fairness「连续」的定义差半点，表现就是玩家侧与统计侧对不上，且极难查
3. **单机游戏里客户端就是权威**——服务端校验的前提是"不信任客户端"，但客户端本来就在玩家手里。为一个不成立的信任边界付双写代价，是纯亏

> 问「为什么不把校验放服务端」时，答案不是"没时间"，是**信任边界不在那里**。

### 关键设计

| 项 | 做法 | 为什么 |
|---|---|---|
| **响应格式** | Intent 本体，**不加信封**，meta 走 `X-SHM-*` 响应头 | body 与 LLM 原始输出、与回放脚本三者字节级同格式：客户端零剥离代码，直接复用「不信任 LLM 输出」的那个解析器；任何一次真实响应可直接另存为回放脚本 |
| **熔断** | Resilience4j，**刻意不加重试** | 上游 10s < 客户端 12s，一次重试最坏 20s 而客户端早已放弃——重试的结果没人接收，只是白烧一次调用。失败时正确的行为是让客户端降级本地 |
| **限流** | Bucket4j，`runId` + IP 双维度，超限返 **429** | 保护的是上游 LLM 配额，不是这台服务器。**429 不是错误，是设计内的降级路径** |
| **缓存** | 指纹分桶 + 同语境最多 3 条台词轮换 | 见下 |
| **可观测** | Micrometer → `/actuator/prometheus` | P50/P99 取代"我手工测了三次" |

### 缓存：一个被真实流量推翻的设计

指纹只取影响决策的量并对连续量分桶（五维每 20 分一桶、置信度三档、`runId` 不入 key）。
缓存整个 Intent 会让**同类玩家听到同一句台词**，而白泽的人格是体验核心之一——
故同一语境保留最多 3 条，命中时随机取一条。

初版规定"未攒满 3 条不命中"。单测全绿，拿同一请求连发 12 次也完美命中。
**但真实游玩 3 局，一次都没命中过**：一局只发 2 次决策请求（共 3 层，F0 是观察层
不走 Provider），F1/F2 预算不同天然是两条指纹，同一指纹要攒满得连打 4 局以上。

改为**边用边攒**：有候选就能用，未满时隔一次仍走 LLM 补充。3 局的命中率
从 0% 变成 33%，长期稳态不变。

> 教训写在 `CacheUnderRealTrafficTest` 里：**验证缓存必须用真实的访问模式。**
> 用循环验证的是"缓存能不能存取"，而缓存真正要回答的是"在我的流量下省不省钱"——
> 前者绿不代表后者成立。

### 数据回流与参数校准（D-24，第八次开工）

上面那两个参数（**桶宽 20 分、候选 3 条**）当初是**拍的**，代码注释里写着「待回流数据校准」。
第八次开工就是来还这笔债的：把每次决策请求的**原始输入**落进 MySQL，
再在历史流水上**重放不同的指纹方案**，回答「换一套会怎样」。

**关键设计是「存什么」**：不能只存指纹字符串，必须存**参与指纹计算的每一个原始字段并拆成列**——
存了指纹只能回答「这个方案命中率多少」，存了原始字段才能回答「换个方案会怎样」。
**存的成本一样，漏存了就永远补不回来。**

**⭐ 模拟器的正确性锚点**：用当前方案模拟出来的结果，**必须逐条重现真实发生过的历史**。
测试的做法是让真的 `IntentCache` 跑一遍序列、记下每次真实结局，再让模拟器重放同一批记录，
断言两串结局逐条相等。**一个模拟器最容易犯的错是算出一个看起来合理但不对的数，
而它恰好有一个天然的验证方式：拿它去预测已经发生过的事。**

**三条硬约束**（都有测试守着）：数据库挂了服务照常返回决策 ·
落库不计入响应时间 · 队列有界且丢弃可观测。

**⭐ 真实数据（16 局 / 32 条）推翻了我原本的假设，而且挖出了一个实现缺陷。**

我原以为是 `floorIndex` 把一局的两次请求切成了两条指纹。数据说不是——
**把 `floor`、`challengeBudget`、`confidence` 三个字段一起去掉，指纹数和命中率纹丝不动**
（它们三者一一对应，但都不是根源）。

**真正的切碎源是 `decisionHistory`**：F1 的历史恒为空、F2 恒非空，
**它本身就是层号的代理**；更糟的是它的**内容**取决于上一层 LLM 选了什么规则，
每局都不同——**等于给每条指纹加了一个随机后缀**。

**⭐⭐ 而桶宽这条线上，一个反常的数据点挖出了真实缺陷。**

模拟报出**桶宽 34 的命中率反而高于桶宽 50 和 100**。「桶越宽合并越多」是常识，
反过来只能说明分桶实现有问题。查下去确实是：

```
bucket(v) = (int)(v / width)    而 100 是闭区间 [0,100] 的上界
→ 满分永远单独占一个只装它自己的桶
桶宽 20：96.8 → 桶 4，100 → 桶 5    差 3.2 分被算成两种玩家
```

**恰恰违背了桶宽 20 的初衷**（「87 和 85 不该算两种玩家」）。
而 32 条里 `buildConcentration == 100` 的有 **26 条（81%）**——专精单一 Build 算出来就是满分，
**这个缺陷几乎每次都命中**。修完：指纹 **15 → 12**，模拟命中率 **28.1% → 46.9%**，单调性恢复。

> ⭐ **一个不符合直觉的数据点，比一百个符合直觉的更值得追。**
> 它不是看代码看出来的——代码那行看一百遍也很正常。
>
> 这是本项目**第一次改动线上指纹的行为**，允许的理由是
> **它是缺陷修复不是参数调优**：分桶函数没有实现它声称的语义。
> **桶宽 20 这个数字本身没动**，仍待校准。现有两条测试守着边界与单调性。
>
> 另一个自我纠正：保真度自检最初**只比总命中率**，而它自己的单元测试当场造出反例——
> 5 条记录里模拟与实测**都是 2 次命中，却发生在不同位置**。
> **一个会因为巧合而通过的校验，比没有校验更危险。** 已改成逐条比对。

### 跑起来

```powershell
# 落库需要 MySQL；不起也能跑，只是不落库（启动日志会有醒目提示）
docker run -d --name shm-mysql -p 3306:3306 `
  -e MYSQL_ROOT_PASSWORD=root -e MYSQL_DATABASE=shm_director mysql:8.0

cd DirectorService
mvn spring-boot:run          # 端口 8080
mvn test                     # 84 个测试（其中 9 条需 Docker，无 Docker 会跳过）
```

聚合分析（只在本机可用，**刻意不加认证**，因为本服务不部署公网）：

```
GET /v1/analytics/summary                          整体情况
GET /v1/analytics/cache/simulate                   ⭐ 换一套指纹方案会怎样
GET /v1/analytics/fingerprint/split-contribution   每个字段把样本切得多碎
```

key 配在 `src/main/resources/application-local.yml`（**已 gitignore**），
或走环境变量注入：

```yaml
shm:
  llm:
    api-key: sk-...
```

**不配 key 也能起**：走占位实现，响应头 `X-SHM-Source: ServerLocal` 如实标注。
**后端不起也不影响游戏**：客户端降级本地 Provider，玩家零感知。

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
| 单元测试（画像 + 护栏 + Provider + 遭遇 + 武器 + JSON + 回放 + 日志契约） | ✅ **63/63 全绿**（2026-08-14 headless 实跑复核） · [`Docs/TestResults.md`](Docs/TestResults.md) |
| 决策链路端到端 | ✅ 控制台 `SHM.DumpDecision` / `SHM.DumpDecisionAsync` |
| 武器切换 + 弓（画像分化的输入源） | ✅ 已完成 · 攻击按 AttackPattern 分发 |
| 敌人四原型 + 遭遇系统消费敌人权重 | ✅ 已完成 · 数据驱动（CSV）· 刷怪点导航网格投影 |
| **闭环端到端可玩** | ✅ 打一局 3 层：真实行为 → 画像 → 决策 → 下层刷怪与规则生效，肉眼可见被针对 |
| `IAIProvider` 四实现（链路 ④） | ✅ **Remote**（决策网关，生产路径）· Local（降级终点）· **Replay**（确定性回放）· Llm（直连，默认不编译） |
| **三级降级链路** | ✅ Provider 失败 → 护栏拒绝 → 安全兜底，每级留日志；**无 key/断网完整可玩** |
| **降级回归三条全部实测** | ✅ ①物理拔网线 ②后端停掉但网络正常 ③后端返 200 但 body 是垃圾——三种都降级本地，整局完整可玩 |
| 决策日志（含护栏前 RawIntent + 溯源） | ✅ 一局结束自动导出 JSON（`schemaVersion` 契约，回放/可视化共用） |
| **导演报告卡（链路 ⑦）** | ✅ 层间弹出「我看到的 → 本层调整 → 白泽台词 → 决策溯源」，读完才开打 |
| **镜界时间轴** | ✅ `SHM.Timeline` 整局回放：想改什么 → 护栏拦没拦 → 实改什么 |
| **AI 导演开/关对照** | ✅ `SHM.Director 0/1`，关闭后退化为固定难度刷怪，用于对照演示 |
| 本局统计（简历数字来源） | ✅ `SHM.Stats`：护栏分道拦截数 · 降级率 · 决策耗时 |
| **Web 决策回放器**（`WebReplay/`，D-21） | ✅ M0–M5 完成并上线 · 护栏前后对照 · 画像雷达图 · 拖拽载入 · Vitest 84 个 · [Live Demo](https://roseri66.github.io/ShanHaiMirror/) |
| **决策数据落库与聚合**（`DirectorService/`，D-24） | ✅ 链路已通 · 原始字段拆列存储 · 指纹方案模拟器（**含正确性锚点：重放必须逐条重现真实历史**）· 三条硬约束有测试守着 · 16 局 / 32 条真实数据已回流（**并据此挖出并修掉一个分桶缺陷**）· ⚠️ **桶宽 20 / 候选 3 两个数字仍待校准** |
| **调试作弊开关**（D-25） | ✅ 三个默认不生效的控制台变量（`shm.Debug.*`）· ⭐ **开着作弊时请求带标记、落库存 `debug_flags` 列** —— 作弊样本与真实样本永久可分。<br>⚠️ 刻意**没有**「敌人伤害」开关：敌人打不动人会让 `SurvivalPressure` 恒为 0，正好废掉最该采的那一维 |

> ⚠️ **画像五维里有两维信息量不足，照实写**：`resourceSurplus` 恒为 50
> （D-09 砍了道具系统，无数据源，**故刻意不入指纹**）；`survivalPressure`
> 实际只有 `{0, 25}` 两个取值 —— 它测的是「有没有濒死过」而不是「挨了多少打」，
> 而 LowHp 事件是闩锁、且全项目没有回血来源，**所以每层最多触发一次**（踩坑 #36）。
> **真正在区分玩家的是 3 维 + 1 bit**，这也解释了为什么把指纹切碎的是
> `decisionHistory` 而不是画像。

变更记录：[`CHANGELOG.md`](CHANGELOG.md)（含 **M5-5 的校准结论**）· 开发过程记录：[`Docs/Sprint开发总结.md`](Docs/Sprint开发总结.md)（八次开工复盘，含设计判断、计划偏离与修复教训）· [`Docs/踩坑记录.md`](Docs/踩坑记录.md)（**36 条**，每条含现象/原因/解法/规则）

> **实测记录（DeepSeek `deepseek-chat`，OpenAI 兼容端点）**：单次决策往返 3.8–5.0s。
> 三次真实调用分别走通了三条路径——① LLM 同时选中互斥规则（弹药↓ + 远程伤害↓，
> 对远程玩家是无解组合）被 **Conflict 护栏拒绝并降级**；② 超时降级；③ 直采通过，
> 台词「箭矢不够用的时候，你还能保持从容吗？」。第 ① 条是"护栏确实在约束 LLM"的实测证据。

**范围与每一条取舍的理由见 [`Docs/DECISIONS.md`](Docs/DECISIONS.md)**，包含 **25 条**决策记录（砍掉什么、为什么砍、代价是什么、以及时间不够时的削减顺序）。MVP 冻结后的每一条扩展都要在 §7 先追一条显式决策——**不追决策就动手，等于自己破自己的规矩**。

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

### LLM 配置：key 在服务端，不在客户端

**客户端不持有任何凭据。** LLM 调用由决策网关 `DirectorService/` 承担（D-23），
客户端只知道网关地址：

```powershell
setx SHM_DIRECTOR_URL "http://localhost:8080"   # 选填，默认就是它
setx SHM_DIRECTOR_TIMEOUT "12"                  # 选填，秒
```

key 配在服务端的 `DirectorService/src/main/resources/application-local.yml`
（**已 gitignore**），或走环境变量注入：

```yaml
shm:
  llm:
    api-key: sk-...
```

```powershell
cd DirectorService
mvn spring-boot:run
```

**后端起不起得来都不影响游戏可玩性**：网关不可达、超时、返回 5xx / 429，
客户端一律降级本地 Provider，日志留痕，玩家零感知。
未配 key 时服务端走占位实现，响应头 `X-SHM-Source: ServerLocal` 如实标注。

> 客户端仍保留一个直连 LLM 的 `FSHMLlmProvider`，但**默认不编译**
> （`ShanHaiMirror.Build.cs` 里 `SHM_DEV_DIRECT_LLM=0`），仅供无后端时调试。
> 生产路径的 prompt 真源是服务端的 `prompt.yaml`，两者允许漂移、不保证一致。

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
