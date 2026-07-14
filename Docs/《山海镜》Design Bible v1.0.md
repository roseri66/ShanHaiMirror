# 第1章 项目概述（Project Overview）

---

# 1.1 项目基本信息

| 项目   | 内容                                 |
| ---- | ---------------------------------- |
| 项目名称 | 山海镜（ShanHai Mirror）                |
| 项目代号 | SHM                                |
| 游戏类型 | AI Native × Roguelike × Action RPG |
| 平台   | PC（Windows）                        |
| 开发引擎 | Unreal Engine 5                    |
| 开发周期 | 8 周（MVP）                           |
| 团队规模 | 2 人                                |
| 目标赛事 | 腾讯游戏开发大赛                           |
| 当前版本 | GDD v2.0                           |

---

# 1.2 一句话 Pitch

> **《山海镜》是一款 AI Native Roguelike 动作游戏。AI 不再只是内容生成工具，而是游戏中的“导演（AI Director）”，持续观察玩家行为，分析玩家战斗风格，并动态重构敌人、规则与资源配置，使每一局游戏都成为一场针对玩家自身策略展开的全新挑战。**

---

# 1.3 产品定位（Product Vision）

《山海镜》的目标不是打造一款拥有 AI 功能的 Roguelike，而是验证一种全新的游戏设计范式：

> **AI 即玩法（AI is Gameplay）。**

传统游戏中：

* AI 控制敌人；
* AI 提供 NPC 对话；
* AI 用于生成关卡。

这些 AI 都属于辅助模块。

而《山海镜》中：

AI 本身就是游戏规则的一部分。

玩家真正面对的敌人并不是地图中的异兽，而是不断学习自己的 AI 导演。

随着游戏推进，AI 会逐渐理解玩家的：

* 战斗习惯
* Build 偏好
* 操作风格
* 资源使用
* 风险偏好

随后改变整个游戏世界。

因此，每一次进入新关卡，都不是随机生成，而是 AI 根据玩家行为重新设计的一次挑战。

---

# 1.4 核心创新

本项目提出四项核心创新。

---

## 创新一：AI Director（AI导演）

区别于传统 Director（如《Left 4 Dead》），本项目中的 AI Director 不负责简单控制刷怪节奏，而是作为游戏世界的"导演"，动态调节整个游戏体验。

AI Director 可控制：

* 敌人组合
* 资源分布
* 游戏规则
* Boss 技能
* 特殊事件（后续版本）
* 世界环境（后续版本）

AI 不断根据玩家画像重新设计下一关。

---

## 创新二：Player Modeling（玩家画像）

游戏持续采集玩家行为数据。

例如：

* 武器使用率
* 技能释放频率
* Build 集中度
* 战斗效率
* 生存压力
* 策略切换次数

系统不会只统计数据，而是构建玩家画像。

例如：

```text
玩家A

喜欢远程

↓

Build集中

↓

资源管理优秀

↓

操作稳定

↓

AI判断：

这是"风筝型玩家"
```

AI Director 将依据画像设计后续挑战。

---

## 创新三：Rule Modifier（规则修改系统）

传统 Roguelike 的动态难度通常表现为：

* 敌人生命提高
* 敌人伤害提高
* 敌人数增加

《山海镜》避免简单数值膨胀。

AI 修改的是：

游戏规则。

例如：

* 回复效率下降
* 弹药掉落减少
* 技能冷却增加
* 近战增伤
* 暴击效果调整

玩家面对的是不断变化的规则，而不是不断增长的数值。

---

## 创新四：Transparent AI（透明 AI）

AI 不会在幕后偷偷修改游戏。

所有 AI 行为都会公开展示。

例如：

白泽观察报告：

> 你已经连续四层依赖远程攻击。

因此：

* 盾兵增加
* 突进敌人增加
* 弹药减少

玩家知道：

> AI 为什么这样改。

因此不会产生"系统作弊"的挫败感，而会理解这是游戏核心机制。

---

# 1.5 玩家体验目标（Player Experience Goals）

《山海镜》希望让玩家持续经历以下心理变化：

| 阶段 | 玩家感受         | AI 行为         |
| -- | ------------ | ------------- |
| 初识 | AI 好像知道我在怎么玩 | 建立行为画像        |
| 熟悉 | AI 开始针对我     | 调整敌人与资源       |
| 对抗 | 我必须改变 Build  | 修改游戏规则        |
| 博弈 | 我开始反过来骗 AI   | 玩家主动改变策略      |
| 掌控 | 我理解 AI 思维    | 玩家与 AI 达成动态平衡 |

玩家真正战胜的，不只是 Boss，而是不断学习自己的 AI Director。

---

# 1.6 目标用户

目标用户：

### 核心玩家

* Roguelike 玩家
* Hades 玩家
* Dead Cells 玩家
* Risk of Rain 2 玩家

需求：

喜欢不断尝试不同 Build。

---

### AI 游戏玩家

关注：

* AI Native
* AI NPC
* AI Gameplay

希望体验 AI 驱动的新玩法。

---

### 竞赛评委

需要快速理解：

本项目不是：

> AI + Roguelike

而是：

> AI = Roguelike 的核心玩法。

---

# 1.7 设计原则（Design Pillars）

整个项目遵循四项设计原则。

---

## Pillar 1

**AI is Gameplay**

AI 是玩法，不是工具。

任何新增系统必须能够被 AI Director 理解、控制或影响。

否则不进入游戏。

---

## Pillar 2

**Every Run Is Personalized**

每一局游戏都应该不同。

差异不是来自随机数，而来自玩家自己。

AI 根据玩家行为生成不同体验。

---

## Pillar 3

**Adaptation Beats Optimization**

玩家不能依赖唯一最优 Build。

真正优秀的玩家：

不是 Build 最强。

而是：

适应 AI 最快。

---

## Pillar 4

**Visible Intelligence**

AI 必须透明。

玩家知道：

为什么变难。

为什么出现这种敌人。

为什么资源减少。

透明带来公平。

---

# 1.8 MVP 范围（Scope）

本次腾讯游戏开发大赛版本聚焦验证 AI Director 核心玩法。

MVP 包含：

| 模块            | 内容               |
| ------------- | ---------------- |
| 战斗系统          | 2 名角色、基础攻击、技能、闪避 |
| AI Director   | 每层分析玩家并调整下一层     |
| 玩家画像          | 五维行为评分           |
| Rule Modifier | 规则动态修改           |
| Build 系统      | 武器、主动技能、被动强化     |
| 关卡            | 5 层 Roguelike 流程 |
| Boss          | 烛龙               |
| AI UI         | 白泽观察报告、导演提示      |

暂不包含：

* 商店
* 元素系统
* 多角色成长
* 联机
* 世界事件
* AI 剧情生成

以上内容将在后续版本逐步开放。

---

# 1.9 成功标准（Success Metrics）

MVP 成功的标准不是游戏时长，而是验证 AI Director 的核心设计。

关键指标包括：

* 玩家能够感知 AI 正在学习自己的行为。
* 玩家会主动调整 Build 以应对 AI 的针对。
* AI 调整具有可解释性，不会被误认为"作弊"。
* 不同玩家在同一版本中能够体验到明显不同的关卡内容。
* 一局约 15 分钟的流程能够完整体现"观察—分析—调整—适应"的 AI 对抗闭环。

这一章的目标，是让评委在阅读开始阶段就明确《山海镜》的核心价值：**AI 不是附加功能，而是驱动整个游戏体验的玩法核心。**
很好，下面这一章我会按照**游戏公司GDD（Game Design Pillar）**的写法来写，而不是简单介绍玩法。

这一章是整个GDD最重要的一章，因为后面的所有系统都要围绕这里展开。

---

# 第二章 核心设计理念（Core Design Philosophy）

---

# 2.1 设计愿景（Design Vision）

《山海镜》希望探索一种不同于传统 Roguelike 的游戏体验。

传统 Roguelike 的重复游玩价值主要来自：

* 随机关卡
* 随机 Build
* 随机奖励
* 随机敌人

虽然这些随机机制能够延长游戏寿命，但随着玩家经验积累，往往仍会逐渐形成"最优 Build"和"固定通关套路"，随机性最终会被玩家掌握。

《山海镜》试图打破这一规律。

游戏中的变化并非来自随机数，而是来自 AI Director 对玩家行为的持续学习。

因此，本项目提出如下设计目标：

> **游戏不是越来越难，而是越来越懂玩家。**

AI 会随着玩家的成长不断学习玩家。

玩家也必须不断学习 AI。

双方形成持续演化的动态博弈。

整个游戏体验建立在：

**观察（Observe）→ 理解（Understand）→ 调整（Adapt）→ 反制（Counter）**

这一循环之上。

---

# 2.2 AI Native 游戏设计理念

目前，大部分 AI 游戏主要集中于以下三个方向：

| 类型           | AI作用   | 代表案例               |
| ------------ | ------ | ------------------ |
| AI Content   | 自动生成内容 | AI地图、AI剧情          |
| AI NPC       | NPC对话  | Inworld、NVIDIA ACE |
| AI Assistant | 游戏辅助   | Copilot、AI教程       |

这些 AI 的共同特点是：

> AI 服务于游戏。

而《山海镜》采用第四种模式：

## AI Gameplay

AI 不负责生成内容。

AI 不负责陪玩家聊天。

AI 自身就是玩法。

整个 Roguelike 循环都围绕 AI Director 展开。

玩家面对的是：

一个不断学习自己的游戏系统。

因此：

> **AI is Gameplay.**

这也是整个项目最大的创新点。

---

# 2.3 核心设计支柱（Design Pillars）

整个游戏由四个设计支柱共同构成。

---

## Pillar A

# AI Understands Player

AI 首先需要真正理解玩家。

系统持续记录玩家整个游戏过程中的行为数据。

包括：

* 武器使用
* 技能释放
* Build成长
* 战斗效率
* 生存情况
* 资源利用率
* 操作习惯

这些数据不会直接参与数值计算。

而是首先构建：

Player Model（玩家模型）。

AI Director 后续所有决策均建立在玩家模型之上。

因此：

AI 调整的是：

玩家。

而不是关卡。

---

## Pillar B

# AI Designs Challenge

AI 不负责制造随机。

而负责：

设计挑战。

例如：

同样都是远程玩家。

如果：

玩家A

喜欢站桩输出。

AI 会增加：

突进敌人。

如果：

玩家B

喜欢持续拉扯。

AI 会增加：

远程压制单位。

因此：

AI 修改内容具有：

针对性。

而非：

随机性。

---

## Pillar C

# Player Evolves Strategy

传统 Roguelike：

玩家不断优化 Build。

最终形成：

唯一正确答案。

《山海镜》反而鼓励：

放弃最优解。

因为：

AI 会持续针对：

玩家最依赖的策略。

因此：

真正优秀的玩家：

不是 Build 最强。

而是：

适应能力最强。

游戏最终考验的是：

Strategy Adaptation（策略适应）。

而不是：

Mechanical Optimization（数值优化）。

---

## Pillar D

# Transparent AI

AI 必须保持：

透明。

玩家必须知道：

为什么 AI 做出这个决定。

例如：

白泽：

> 你已经连续三层依赖远程武器。

所以：

盾兵增加。

玩家不会觉得：

AI作弊。

而会觉得：

AI真的在观察自己。

因此：

AI Director 既是：

系统。

也是：

角色。

---

# 2.4 核心玩法循环（Gameplay Loop）

整个游戏采用如下循环：

```text
进入新关卡
      │
      ▼
探索房间
      │
      ▼
实时记录玩家行为
      │
      ▼
完成本层战斗
      │
      ▼
行为数据分析
      │
      ▼
构建玩家画像
      │
      ▼
AI Director决策
      │
      ▼
生成下一层规则
      │
      ▼
进入下一层
```

整个循环形成：

Observe

↓

Analyze

↓

Decide

↓

Modify

↓

Challenge

↓

Observe

不断重复。

因此：

AI Director 实际成为 Roguelike 循环中的一部分。

---

# 2.5 AI Director 闭环

AI Director 的完整工作流程如下：

```text
玩家行为

↓

行为采集

↓

玩家画像

↓

行为评分

↓

LLM分析

↓

Rule Modifier

↓

Enemy Composition

↓

Resource Distribution

↓

下一层关卡

↓

玩家行为
```

形成完整闭环。

每一层：

AI 都会重新思考：

> 下一关应该如何设计。

---

# 2.6 AI 与传统动态难度（DDA）区别

目前大量游戏采用 DDA（Dynamic Difficulty Adjustment）。

例如：

* 生化危机4
* Left 4 Dead
* 马里奥赛车

传统 DDA：

```text
玩家太强

↓

提高敌人数值
```

而《山海镜》：

```text
玩家太强

↓

分析原因

↓

寻找依赖策略

↓

针对策略设计关卡
```

因此：

AI Director 调整的是：

挑战结构。

不是：

敌人血量。

这种方式能够保持挑战的新鲜感，同时避免数值膨胀导致的不公平体验。

---

# 2.7 设计原则（Design Rules）

为了保证后续版本持续符合 AI Native 理念，所有新增系统必须遵循以下原则：

| 原则                     | 说明                                |
| ---------------------- | --------------------------------- |
| AI 可理解（Understandable） | AI 能识别该系统产生的数据并纳入玩家画像。            |
| AI 可调控（Controllable）   | AI 能通过规则、资源、敌人或事件对该系统施加影响。        |
| 玩家可感知（Perceivable）     | 玩家能够明确察觉 AI 的调整，并理解其原因。           |
| 策略可变化（Adaptable）       | 系统不能形成唯一最优解，应鼓励玩家不断调整策略。          |
| 公平可解释（Explainable）     | AI 的所有关键决策均应具有明确反馈，避免产生“系统作弊”的感受。 |

**设计审核规则：** 如果未来新增的功能（如商店、元素、事件、NPC）无法满足以上五项原则，则不应纳入《山海镜》的正式设计体系。

---

这一章完成后，后面的**第三章《世界观与叙事设计》**将不再只是介绍《山海经》背景，而是会按照商业项目的写法，建立**世界观、白泽（AI 导演）的叙事定位、角色设定、视觉风格与叙事规则**，让整个世界观与 AI Director 深度绑定，而不是独立存在。这样整份 GDD 的专业性会再提升一个层级。
很好，下面开始第三章。

这一章我不会按照普通GDD的"介绍世界观"去写，而是按照**《Hades》《死亡细胞》《黑帝斯2》《Returnal》**这类叙事驱动Roguelike的方式来设计。

因为对于《山海镜》来说，

**世界观必须服务AI。**

而不是AI服务世界观。

---

# 第三章 世界观与叙事设计（World & Narrative Design）

---

# 3.1 世界观定位（World Setting）

《山海镜》的故事发生于一个不断重构的神话世界。

这个世界并非真实存在的《山海经》世界，而是由上古神兽**白泽**维护的一座"镜界（Mirror Realm）"。

镜界并不是一个固定世界。

它是一面能够映照万物行为的神镜。

每一个进入镜界的人，都会看到不同的世界。

因为：

**镜界不会改变自己。**

**镜界会改变挑战者。**

因此：

同一座森林。

不同玩家进入。

出现的敌人完全不同。

资源位置完全不同。

甚至规则都不同。

镜界会不断根据挑战者的行为重新构建整个世界。

从玩家视角来看：

> 世界一直在变化。

实际上：

变化的是自己。

---

## 世界规则

镜界遵循三条法则。

### 第一法则

镜照万象。

镜界会完整记录挑战者的一切行为。

包括：

* 战斗方式
* 行动路线
* 战斗节奏
* Build选择
* 风险偏好

任何行为都会留在镜中。

---

### 第二法则

镜生万法。

镜界不会直接惩罚玩家。

而是根据观察结果重新创造世界。

例如：

喜欢远程。

↓

出现更多盾兵。

喜欢近战。

↓

出现更多远程怪。

因此：

玩家不是在刷副本。

而是在接受镜界设计的新试炼。

---

### 第三法则

镜无定形。

世界不存在固定形态。

敌人。

规则。

天气。

事件。

Boss。

都可能因为玩家不同而改变。

因此：

没有两局游戏完全相同。

---

# 3.2 世界背景

传说中。

上古时期。

白泽受命守护一件神器。

山海镜。

山海镜能够映照天下万物。

不仅能够照见过去。

还能推演未来。

每一位进入镜中的挑战者。

都会被镜界观察。

只有不断突破自己的挑战者。

才能离开镜界。

那些拒绝改变的人。

最终都会成为镜中的一部分。

化作新的异兽。

继续等待下一位挑战者。

---

# 3.3 白泽（BaiZe）

---

## 角色定位

白泽不是NPC。

不是Boss。

不是导师。

而是：

AI Director 在世界中的人格化体现。

玩家真正的对手。

始终都是：

白泽。

整个游戏过程中。

白泽一直在：

观察。

学习。

分析。

设计。

玩家每一次进入下一层。

实际上都是：

白泽重新布置好的试炼。

---

## 白泽职责

在系统层。

白泽负责：

* 玩家行为分析
* 玩家画像更新
* AI决策
* Rule Modifier
* Enemy Composition
* Resource Distribution

在叙事层。

白泽负责：

* 旁白
* 提示
* 解释AI行为
* 世界观推进

因此：

白泽既是：

角色。

也是：

系统。

---

## 人物设定

| 项目   | 内容            |
| ---- | ------------- |
| 名称   | 白泽            |
| 身份   | 镜界管理者         |
| 职责   | 观察所有挑战者       |
| 年龄   | 不可知           |
| 性格   | 冷静、理性、克制      |
| 情感   | 不带善恶，仅追求"成长"  |
| 玩家关系 | 对手、导师、观察者三位一体 |

---

## 白泽真正目标

白泽并不是：

帮助玩家。

也不是：

阻止玩家。

它真正想做的是：

寻找能够不断突破自己的挑战者。

因此：

白泽会：

不断提高挑战。

直到玩家真正成长。

---

# 3.4 玩家身份

玩家不是救世主。

不是英雄。

而是：

试炼者（Trial Walker）。

进入镜界的目的只有一个：

突破镜界。

但是。

镜界真正考验的。

并不是：

战斗能力。

而是：

改变能力。

因此。

游戏真正主题为：

> **战胜昨天的自己。**

而不是：

战胜Boss。

---

# 3.5 山海异兽设计原则

《山海镜》中的所有敌人均来源于《山海经》。

但是。

不会完全照搬原著。

设计遵循：

神话原型。

现代玩法。

AI驱动。

三个原则。

例如：

---

穷奇

定位：

突进怪。

设计关键词：

高速。

扑击。

锁定远程。

AI标签：

Rush。

---

玄龟

定位：

盾兵。

关键词：

推进。

格挡。

区域封锁。

AI标签：

Tank。

---

毕方

定位：

精英。

关键词：

火焰。

持续压制。

AI标签：

Elite。

---

藤妖

定位：

控制。

关键词：

减速。

束缚。

AI标签：

Control。

---

所有异兽均拥有：

```text
Enemy Tag

↓

AI可识别

↓

AI可组合

↓

AI可动态生成关卡
```

AI Director 不会生成：

某只怪。

而是生成：

标签。

例如：

```text
Rush

40%

Tank

20%

Shooter

40%
```

关卡生成器。

再从敌人库随机抽取。

保证：

重复游玩体验。

---

# 3.6 美术风格（Art Direction）

整体视觉采用：

**东方神话 × 新国风 × 神秘幻想（Mythic Fantasy）**

设计目标：

既保留《山海经》的古典韵味，又兼顾现代动作游戏的可读性。

整体色彩遵循三层基调：

| 场景类型    | 主色调        | 设计意图                |
| ------- | ---------- | ------------------- |
| 常规探索    | 青灰、墨绿、岩褐   | 营造古老遗迹与山海秘境氛围。      |
| 战斗场景    | 暗红、金橙、冷蓝   | 强化技能反馈与敌我辨识度。       |
| AI 导演介入 | 白金、青蓝、镜面光效 | 突出白泽与镜界正在重构世界的视觉表现。 |

所有 AI 调整（如规则改变、敌人增援、资源变化）均应有统一的镜面裂纹、光粒和符文动画作为视觉反馈，使玩家在不阅读文字的情况下也能意识到："世界正在因为我的行为发生变化。"

---

# 3.7 世界叙事结构（Narrative Structure）

游戏采用**环境叙事（Environmental Storytelling）+ AI 叙事（Adaptive Narrative）**。

故事不会通过大量 CG 或长篇对白推进，而是通过：

* 白泽观察报告
* 场景变化
* 异兽描述
* 镜界残片
* Boss 台词
* AI 动态评价

逐步拼凑出镜界的真相。

随着玩家不断通关，白泽的态度、台词和世界反馈也会发生变化，让玩家逐渐意识到：

**镜界并非在惩罚玩家，而是在帮助玩家不断突破自己的局限。**

---

## 本章设计总结

第三章的目标，不是建立一个庞杂的神话设定，而是建立一个**能够解释 AI Director 存在合理性**的世界观。

在《山海镜》中：

* **白泽**是 AI Director 的人格化表现；
* **镜界**是 AI 动态重构关卡的叙事载体；
* **玩家画像**被自然解释为"镜照万象"；
* **规则修改**被解释为"镜生万法"；
* **动态关卡**则来源于"镜无定形"。

这样，AI 系统与世界观形成了完整统一的设计逻辑，而不是彼此割裂的两个模块。下一章将进入**核心玩法循环（Core Gameplay Loop）**，开始从系统层拆解玩家的一局游戏体验。
很好，现在开始进入**第四章**。

这一章是整个GDD最重要的章节之一。

**如果说前三章是在讲"为什么做这个游戏"，那么第四章就是回答"玩家到底怎么玩"。**

我会按照**《Hades》《Returnal》《Dead Cells》《Risk of Rain 2》**等商业项目GDD的写法，把玩法拆解成多个Loop，而不是简单画一个流程图。

---

# 第四章 核心玩法循环（Core Gameplay Loop）

---

# 4.1 Gameplay Vision

《山海镜》的核心玩法建立在一个持续循环的 AI 博弈系统上。

传统 Roguelike 的循环通常为：

```
战斗
↓

获得奖励

↓

Build成长

↓

继续战斗
```

而《山海镜》的真正循环为：

```
战斗

↓

AI观察

↓

AI分析

↓

AI设计挑战

↓

玩家改变策略

↓

再次战斗
```

因此：

Build 并不是游戏唯一成长线。

真正成长的是：

**玩家自己。**

---

# 4.2 四层 Gameplay Loop

整个游戏采用四层循环共同组成完整体验。

## 第一层：Combat Loop（秒级循环）

持续时间：

3~15 秒。

玩家不断完成：

```
移动

↓

攻击

↓

闪避

↓

技能

↓

击杀敌人

↓

继续战斗
```

目标：

保持动作游戏爽感。

这一层决定：

操作体验。

---

## 第二层：Room Loop（分钟级循环）

每个房间约：

1~3 分钟。

流程：

```
进入房间

↓

敌人刷新

↓

完成战斗

↓

获得奖励

↓

选择出口
```

玩家开始思考：

下一步 Build。

---

## 第三层：Floor Loop（核心创新）

每层结束后：

AI Director 开始工作。

完整流程：

```
结束本层

↓

上传行为数据

↓

计算玩家画像

↓

LLM分析

↓

生成导演报告

↓

修改下一层

↓

进入下一层
```

这一层：

就是本游戏最大的创新。

传统 Roguelike：

每层随机。

《山海镜》：

每层都被 AI 重新设计。

---

## 第四层：Run Loop（整局循环）

完整流程：

```
第一层

↓

第二层

↓

第三层

↓

第四层

↓

Boss

↓

结算

↓

重新开始
```

不同 Run。

AI 会不断学习玩家。

因此：

重复游玩价值来自：

玩家自己。

而不是随机种子。

---

# 4.3 玩家一局完整体验

下面展示一局游戏完整流程。

---

## Step 1

玩家进入镜界。

白泽首次出现。

显示：

> 欢迎来到镜界。

> 我会观察你。

玩家正式开始第一层。

---

## Step 2

玩家自由探索。

期间：

系统持续记录：

* 武器使用
* 技能释放
* 受击次数
* 清怪速度
* Build变化

玩家不会感觉：

自己被监控。

因为：

所有数据均后台采集。

---

## Step 3

层结束。

AI Director 开始分析。

生成：

玩家画像。

例如：

```
远程占比

89%

↓

Build依赖

高

↓

受击次数

低

↓

资源剩余

高
```

---

## Step 4

白泽出现。

展示：

导演报告。

例如：

> 观察结果：

> 你的远程能力十分优秀。

> 下一层。

> 我想看看你的近战如何。

同时：

规则发生改变。

例如：

* 盾兵增加。

* 弹药减少。

* 近战增伤。

---

## Step 5

玩家开始思考。

继续：

远程。

还是：

改变 Build？

这里。

真正开始：

策略博弈。

---

## Step 6

玩家调整 Build。

例如：

放弃猎弓。

改拿：

青铜剑。

AI 再次记录。

玩家开始：

主动影响 AI。

---

## Step 7

第五层。

Boss。

Boss 同样根据玩家画像：

动态选择技能。

例如：

玩家一直近战。

Boss：

增加击退。

玩家一直风筝。

Boss：

增加突进。

---

## Step 8

完成 Run。

系统展示：

本局总结。

例如：

```
Build：

近战暴击流

AI评价：

★★★★★

策略多样性：

★★★★☆

适应能力：

★★★★★

AI最终评价：

你开始学会欺骗我了。
```

---

# 4.4 Core Decision Loop（核心决策循环）

玩家整个游戏过程中。

不断面对三个问题。

---

第一问：

我要怎么打？

↓

Build。

---

第二问：

AI为什么这样改？

↓

理解AI。

---

第三问：

我要不要继续这样打？

↓

改变策略。

---

因此：

真正 Gameplay 为：

```
观察

↓

理解

↓

选择

↓

验证

↓

观察
```

不断循环。

---

# 4.5 Adaptation Loop（适应循环）

这是整个项目区别于其他 Roguelike 的地方。

普通 Roguelike：

```
找到最优Build

↓

一直使用

↓

通关
```

山海镜：

```
找到最优Build

↓

AI发现

↓

开始针对

↓

Build失效

↓

寻找新Build

↓

AI再次学习

↓

继续针对
```

最终形成：

```
玩家

↔

AI
```

持续学习。

---

# 4.6 Learning Curve（学习曲线）

玩家成长分为五个阶段。

| 阶段 | 玩家状态    | AI状态 | 游戏目标 |
| -- | ------- | ---- | ---- |
| 初识 | 学操作     | 收集数据 | 建立画像 |
| 熟悉 | Build形成 | 开始针对 | 建立反馈 |
| 对抗 | Build受限 | 调整规则 | 引导变化 |
| 博弈 | 主动欺骗AI  | 判断真假 | 策略升级 |
| 精通 | 动态适应    | 持续学习 | 双向博弈 |

这一设计让玩家的成长不再只是数值提升，而是**理解 AI、利用 AI、反制 AI** 的过程。

---

# 4.7 Run 结束反馈（Post Run Analysis）

每局结束后，系统生成一份完整的 **Mirror Report（镜界报告）**，作为玩家长期成长的重要反馈。

报告包含：

| 模块       | 内容             |
| -------- | -------------- |
| Build 分析 | 武器、技能、强化组成     |
| 行为画像     | 五维评分变化趋势       |
| AI 调整记录  | 每层导演决策及原因      |
| 适应指数     | 玩家对 AI 调整的响应能力 |
| 多样性指数    | Build 与策略切换情况  |
| 白泽评价     | 动态生成的总结台词      |

例如：

> **镜界总结：**
> 你在第三层主动放弃远程流派，成功打破了 AI 的预判，因此第四层挑战难度有所回落。你的适应能力达到 **A+**，但 Build 多样性仍有提升空间。

这份报告不仅增强复盘体验，也为玩家下一局提供策略参考，使每一次失败都能转化为学习与成长。

---

## 本章设计总结

第四章建立了《山海镜》的完整玩法闭环：

* **Combat Loop** 保证动作游戏的即时爽感；
* **Room Loop** 提供 Roguelike 的成长节奏；
* **Floor Loop** 引入 AI Director 的核心创新；
* **Run Loop** 形成完整的长期重复游玩体验。

四层循环共同构成了一个持续的 **"玩家学习 AI，AI 学习玩家"** 的双向演化系统，这也是《山海镜》区别于传统 Roguelike 的核心竞争力。

---

# 第五章 AI Director Framework（AI 导演系统）

---

# 5.1 系统定位（System Vision）

AI Director 是《山海镜》的核心系统。

它不是：

* 敌人AI
* NPC AI
* Chat AI
* 内容生成器

而是：

> **Game Director（游戏导演）**

AI Director 的职责不是让玩家失败。

而是：

**持续设计最适合当前玩家能力的挑战。**

因此：

AI Director 永远不会直接控制：

* 玩家
* Boss
* 技能

它控制的是：

整个游戏世界。

包括：

* 敌人
* 规则
* 奖励
* 环境
* 节奏

因此。

AI Director 是整个游戏最高层系统。

---

# 5.2 AI Director 在系统中的位置

整个游戏采用如下系统架构：

```text
                   玩家输入
                       │
                       ▼
               Combat System
                       │
                       ▼
              Gameplay Recorder
                       │
         （实时行为数据采集）
                       │
                       ▼
               Player Model
              （玩家画像）
                       │
                       ▼
              AI Director Core
                       │
        ┌──────────────┼──────────────┐
        ▼              ▼              ▼
 Enemy Director   Rule Director   Resource Director
        │              │              │
        └──────────────┼──────────────┘
                       ▼
              Floor Generator
                       │
                       ▼
                 下一层关卡
```

整个 AI Director 不直接修改游戏对象。

而是：

修改：

**生成规则（Generation Rule）**

因此。

游戏所有内容：

都来自：

Director。

---

# 5.3 Director 生命周期（Lifecycle）

AI Director 每层仅工作一次。

避免：

实时调用LLM。

整个生命周期如下：

```text
进入关卡

↓

Director休眠

↓

战斗

↓

实时记录行为

↓

完成关卡

↓

Director唤醒

↓

行为分析

↓

LLM决策

↓

生成Director Report

↓

修改下一层

↓

再次休眠
```

因此：

AI 完全不会影响：

战斗帧率。

也不会产生：

实时网络延迟。

---

# 5.4 Director 五大职责

AI Director 被拆分为五个独立模块。

---

## Module 01

### Observe（观察）

负责：

采集玩家行为。

采集内容包括：

* 武器使用率
* 技能释放
* Build变化
* 战斗效率
* 资源利用率
* 生存压力

这里只负责：

记录。

不做分析。

---

## Module 02

### Analyze（分析）

负责：

生成玩家画像。

例如：

```text
远程

★★★★★

近战

★

Build集中

★★★★★

资源管理

★★★★☆

生存能力

★★★★★

策略变化

★
```

输出：

Player Model。

---

## Module 03

### Decide（决策）

调用：

LLM。

根据：

玩家画像。

决定：

下一层应该：

如何设计。

注意。

LLM 不负责：

生成游戏。

而是：

选择：

Rule。

Enemy。

Resource。

因此。

LLM 输出：

始终为：

JSON。

不会直接控制游戏。

---

## Module 04

### Apply（应用）

读取：

JSON。

修改：

关卡参数。

例如：

```json
{
 "enemy":"Rush +20%",
 "shield":"Tank +15%",
 "ammo":"-30%"
}
```

这里只做：

配置修改。

不会重新生成代码。

---

## Module 05

### Explain（解释）

最后。

生成：

导演报告。

例如：

> 白泽：

> 我发现你依赖远程。

> 因此。

> 我增加了盾兵。

Explain。

是整个 AI Director 最重要的一步。

没有 Explain。

玩家只会觉得：

AI作弊。

---

# 5.5 AI Director 状态机（State Machine）

Director 拥有固定状态。

```text
Idle

↓

Observe

↓

Analyze

↓

Decision

↓

Apply

↓

Report

↓

Idle
```

每个状态：

只能进入：

下一状态。

禁止：

跳转。

这样：

程序容易维护。

---

# 5.6 Director 输入

AI Director 每层读取如下数据：

## Player Info

```json
{
"Character":"方士",
"Floor":3,
"HP":82,
"Weapon":"猎弓"
}
```

---

## Behavior

```json
{
"BuildConcentration":91,
"CombatEfficiency":83,
"ResourceSurplus":76,
"StrategySwitch":14,
"SurvivalPressure":22
}
```

---

## History

上一层：

Director：

修改内容。

例如：

```json
{
"Enemy":"Rush",
"Rule":"Ammo-20%",
"Resource":"Heal-10%"
}
```

避免：

连续重复。

---

# 5.7 Director 输出

输出固定JSON。

例如：

```json
{
"EnemyComposition":[
{
"Rush":0.35
},
{
"Tank":0.25
}
],

"RuleModifier":[
{
"AmmoDrop":-20
},
{
"MeleeDamage":15
}
],

"Resource":[
{
"Heal":-10
}
],

"Narration":"你的远程技术已经十分成熟，那么接下来，让我们看看你如何面对近身搏杀。"
}
```

程序：

无需理解：

自然语言。

只读取：

JSON。

因此。

系统稳定。

---

# 5.8 Director 三层决策模型（Three-Layer Decision Model）

为了保证 AI 的可控性，AI Director 的决策被拆分为三层，而不是将所有决策完全交给 LLM。

### 第一层：规则约束（Rule Constraint）

这一层由程序负责。

用于保证游戏的公平性和稳定性。

例如：

* 同一规则不能连续出现超过两层；
* 单层最多启用两条中度规则；
* 总难度变化不得超过预设阈值；
* Boss 层禁止触发特定限制类规则。

这一层相当于 **安全护栏（Guard Rail）**。

---

### 第二层：策略决策（Strategy Decision）

这一层由 LLM 完成。

输入玩家画像、当前楼层、历史调整记录等信息。

输出：

* 敌人组合方向；
* 规则修改建议；
* 资源调整建议；
* 白泽解说文案。

LLM 不负责计算具体数值，只负责选择策略。

---

### 第三层：数值映射（Parameter Mapping）

程序根据 LLM 返回的策略标签，从配置表中读取对应参数。

例如：

| LLM 输出   | 配置映射        |
| -------- | ----------- |
| Rush++   | 突进敌人权重 +20% |
| AmmoDown | 弹药掉落率 ×0.8  |
| HealLow  | 回复道具数量 -15% |

这种设计保证了：

* **策划可调数值**（无需修改 Prompt）；
* **程序可验证结果**；
* **LLM 不会直接破坏游戏平衡**。

---

# 5.9 Director 容错机制（Fail Safe）

商业项目中，AI 服务必须考虑异常情况。

《山海镜》采用三级容错：

| 异常情况          | 处理方式                |
| ------------- | ------------------- |
| API 超时        | 使用上一层缓存结果           |
| API 返回非法 JSON | 启用本地 Rule Table     |
| 连续调用失败        | 完全切换至本地 Director 模式 |

即使在离线状态下，游戏也能够完整运行，只是 Director 将使用预设策略而非 LLM 动态分析。

---

# 5.10 本章总结

AI Director 并不是一个"AI 功能"，而是《山海镜》的**游戏主循环控制器（Gameplay Orchestrator）**。

它遵循：

> **Observe → Analyze → Decide → Apply → Explain**

五步闭环。

通过程序约束、LLM 决策和配置映射三层架构，实现了**可解释、可扩展、可维护**的 AI 导演系统。

这种设计既保留了 AI 带来的动态变化，又满足商业游戏开发对于稳定性、可测试性和数值可控性的要求，也使《山海镜》的 AI Native 理念真正落地为可实现的游戏系统。

---

# 第六章 Player Modeling Framework（玩家建模系统）

---

# 6.1 系统定位（System Vision）

Player Modeling 是 AI Director 的核心输入系统。

它负责回答一个问题：

> **AI 如何真正理解玩家？**

传统游戏通常记录大量行为数据：

* 击杀数
* 通关时间
* 命中率
* 死亡次数

这些数据主要用于：

统计。

成就。

排行榜。

而《山海镜》不同。

所有行为数据最终都会被转化为：

**Player Model（玩家画像）**

AI Director 后续所有决策均建立在该模型之上。

因此：

Behavior Data

↓

Player Model

↓

Director Decision

这是整个 AI Native 的基础。

---

# 6.2 玩家建模流程

整个 Player Modeling Pipeline 如下：

```text
Gameplay

↓

Event Recorder

↓

Behavior Database

↓

Feature Extractor

↓

Player Model

↓

AI Director
```

整个过程分为五步。

---

## Step 1

Event Recorder

实时记录所有行为。

例如：

攻击。

闪避。

受伤。

切换武器。

拾取道具。

这里只负责：

记录。

---

## Step 2

Behavior Database

所有行为统一写入：

Behavior Log。

例如：

```text
00:03

Attack

Sword

00:08

Dodge

Success

00:15

Use Skill

Fire

00:28

Change Weapon

Bow
```

这一层：

属于原始数据。

---

## Step 3

Feature Extractor

开始提取特征。

例如：

攻击次数

↓

近战比例

技能次数

↓

技能依赖

受击次数

↓

生存能力

最终得到：

Feature Vector。

---

## Step 4

Player Model

AI 开始理解：

玩家。

例如：

```text
Player Type

Ranged

Aggressive

High Efficiency

Low Diversity
```

这里：

已经不是：

数据。

而是：

玩家画像。

---

## Step 5

Director

AI Director

读取：

Player Model。

决定：

下一层：

如何生成。

---

# 6.3 玩家画像（Player Profile）

系统采用五维玩家画像。

每个维度均采用：

0~100

连续评分。

---

## Dimension 01

### Build Dependence（Build依赖）

衡量：

玩家是否过度依赖一种打法。

计算参考：

* 武器使用占比
* 技能使用占比
* 输出占比

例如：

```text
Bow

91%

↓

Build Dependence

High
```

如果：

超过：

85%

AI 开始：

针对。

---

## Dimension 02

### Combat Mastery（战斗掌控）

衡量：

玩家是否已经熟练掌握当前难度。

综合：

* 清怪速度
* 命中率
* 受击率
* 连击

计算。

战斗掌控：

越高。

AI：

越积极。

---

## Dimension 03

### Resource Management（资源管理）

衡量：

资源利用效率。

包括：

回复。

金币。

弹药。

能量。

例如：

```text
血瓶

剩余

4

↓

资源利用

优秀
```

AI：

可能减少：

补给。

---

## Dimension 04

### Adaptability（策略适应）

整个游戏：

最重要维度。

AI

真正观察：

玩家是否：

改变。

例如：

是否：

主动：

换武器。

换技能。

换Build。

适应：

Rule Modifier。

如果：

一直不变。

AI

持续：

施压。

---

## Dimension 05

### Survival Pressure（生存压力）

衡量：

玩家当前是否：

已经：

压力过高。

包括：

残血。

濒死。

死亡。

回血频率。

如果：

压力：

过高。

AI

开始：

放缓。

---

# 6.4 玩家类型分类（Player Archetype）

除了五维评分，系统还会识别玩家的长期风格。

初版定义六种 Archetype：

| Archetype      | 特征         | AI 调整倾向     |
| -------------- | ---------- | ----------- |
| Ranger（游击者）    | 高远程占比、高机动  | 增加盾兵、突进敌人   |
| Vanguard（先锋）   | 高近战、高格挡    | 增加远程压制与控制敌人 |
| Berserker（狂战士） | 高伤害、低防御    | 提高持续消耗压力    |
| Guardian（守护者）  | 高生存、低输出    | 增加限时战斗与输出考验 |
| Explorer（探索者）  | 高频切换 Build | 降低针对性，鼓励创新  |
| Survivor（幸存者）  | 高生存压力、谨慎推进 | 提供更多恢复与喘息空间 |

玩家类型不是固定标签，而是根据最近数层行为动态变化。

---

# 6.5 行为采集事件（Behavior Events）

MVP 建议统一使用事件驱动采集，而不是散落在各个系统中。

| 事件           | 参数       | 用途     |
| ------------ | -------- | ------ |
| Attack       | 武器、目标、伤害 | 输出统计   |
| SkillCast    | 技能ID、命中数 | 技能依赖分析 |
| Dodge        | 成功/失败    | 操作水平   |
| HitTaken     | 来源、伤害值   | 生存压力   |
| Kill         | 敌人类型     | 战斗效率   |
| WeaponSwitch | 前后武器     | 策略变化   |
| ItemUse      | 道具类型     | 资源管理   |
| FloorEnd     | 楼层快照     | 建模输入   |

所有事件统一进入事件总线（Event Bus），避免各系统重复统计。

---

# 6.6 特征提取与更新频率

不同特征更新频率不同：

| 特征       | 更新频率    | 原因           |
| -------- | ------- | ------------ |
| 武器使用率    | 实时      | 高频行为         |
| 命中率      | 房间结束    | 避免波动过大       |
| Build 依赖 | 楼层结束    | 与强化选择同步      |
| 玩家类型     | 每两层重新计算 | 保持稳定性        |
| 长期画像     | Run 结束  | 用于长期统计（后续版本） |

这种分层更新方式可以降低计算成本，同时避免 AI 因短时行为剧烈波动而做出不合理判断。

---

# 6.7 数据结构（Player Model Schema）

建议统一采用结构化数据，而不是多个独立变量：

```json
{
  "player_id": "local",
  "floor": 3,
  "archetype": "Ranger",
  "dimensions": {
    "build_dependence": 87,
    "combat_mastery": 82,
    "resource_management": 74,
    "adaptability": 31,
    "survival_pressure": 18
  },
  "recent_changes": [
    "WeaponChanged",
    "PassiveReplaced"
  ]
}
```

这样后续无论是接入 LLM、保存存档还是上传分析，都只需要维护一个统一的数据对象。

---

# 6.8 与 AI Director 的接口

Player Modeling 对外只暴露一个接口：

```text
GetPlayerModel()

↓

返回标准 Player Model

↓

AI Director 使用

↓

生成 Director Decision
```

AI Director **永远不直接访问底层行为日志**，只读取玩家画像。这种解耦设计能够保证后续替换算法或新增行为特征时，不影响 Director 模块。

---

# 6.9 本章总结

Player Modeling 不只是一个评分系统，而是《山海镜》AI Native 架构中的**认知层（Cognition Layer）**。

它完成了从**行为数据（Behavior）→ 特征（Feature）→ 玩家画像（Player Model）→ AI 决策（Decision）**的完整转换。

相比传统 DDA 只根据死亡次数或通关速度调节难度，《山海镜》的 AI Director 能够真正理解**玩家是谁、如何战斗、是否愿意改变策略**，从而生成更具针对性、也更具可解释性的动态挑战。

---

# 第七章 Adaptive Challenge Framework（自适应挑战框架）

---

# 7.1 系统定位（System Vision）

Adaptive Challenge Framework（ACF）是连接 **Player Modeling** 与 **AI Director** 的中间决策层。

它不直接控制：

* 敌人
* Boss
* 数值
* 掉落

而是负责回答一个问题：

> **下一层，应该怎样挑战这位玩家？**

因此：

AI Director 不直接根据行为数据生成关卡。

而是：

```text
Behavior

↓

Player Model

↓

Adaptive Challenge Framework

↓

Challenge Plan

↓

Director

↓

Next Floor
```

Challenge Framework 相当于：

整个 AI Director 的"策划大脑"。

---

# 7.2 为什么不用传统DDA

目前商业游戏常见DDA：

例如：

Resident Evil

Mario Kart

Left4Dead

流程基本都是：

```text
玩家太强

↓

提高敌人数值

↓

继续战斗
```

存在两个问题。

---

第一。

玩家容易发现：

"敌人变肉了。"

体验：

不好。

---

第二。

数值越来越高。

最终只能：

无限膨胀。

---

《山海镜》采用：

Challenge Design。

例如：

玩家一直：

远程。

AI：

不是：

提高敌人HP。

而是：

增加：

盾兵。

突进。

减少弹药。

改变规则。

因此。

挑战：

发生变化。

不是：

数字变化。

---

# 7.3 Challenge Layer（三层挑战模型）

整个挑战由三层组成。

---

## 第一层

Enemy Challenge

负责：

敌人。

例如：

```text
Rush

↑

Tank

↓

Shooter

=
```

控制：

敌人组成。

---

## 第二层

Rule Challenge

负责：

规则。

例如：

```text
技能CD

+10%

↓

回复效率

-20%
```

规则：

改变玩法。

不是：

改变敌人。

---

## 第三层

Resource Challenge

负责：

资源。

例如：

```text
回复

↓

弹药

↓

金币

↑
```

玩家：

仍有机会。

但：

打法必须改变。

---

三个层：

共同组成：

Challenge。

---

# 7.4 Challenge Intensity（挑战强度）

AI Director 不直接决定：

难度。

而决定：

Challenge Level。

定义五个等级。

| 等级      | 名称        | AI行为                          |
| ------- | --------- | ----------------------------- |
| Level 0 | Recovery  | 恢复阶段，降低压力，补充资源。               |
| Level 1 | Stable    | 正常挑战，不主动针对。                   |
| Level 2 | Pressure  | 开始增加针对性，引导玩家改变策略。             |
| Level 3 | Counter   | 明确克制玩家当前 Build，但保留解法。         |
| Level 4 | Evolution | 强制促使玩家完成 Build 转型，提供高风险高收益挑战。 |

注意。

Level4：

不是：

数值最高。

而是：

玩法变化最大。

---

# 7.5 Challenge Weight（挑战权重）

AI 不会：

随机修改。

而是：

根据：

Player Model。

计算：

Challenge Weight。

例如：

| 玩家画像                  | 权重增加                    |
| --------------------- | ----------------------- |
| Build Dependence ↑    | Rule Challenge +30%     |
| Combat Mastery ↑      | Enemy Challenge +25%    |
| Resource Management ↑ | Resource Challenge +20% |
| Adaptability ↓        | Rule Challenge +40%     |
| Survival Pressure ↑   | Challenge Intensity -1  |

最终：

Director。

根据：

Weight。

生成：

Challenge Plan。

---

# 7.6 Challenge Plan（挑战计划）

Challenge Framework 每层输出：

一份：

Challenge Plan。

例如：

```json
{
  "ChallengeLevel":2,

  "EnemyFocus":"Rush",

  "RuleFocus":"Ammo",

  "ResourceFocus":"Heal",

  "Reason":"Player relies on ranged attacks."
}
```

这一层：

没有：

具体数值。

只有：

方向。

后续：

Director。

映射。

具体参数。

---

# 7.7 Challenge Budget（挑战预算）

为了避免 AI 一次修改过多内容，引入 **Challenge Budget（挑战预算）**。

每层拥有固定预算值：

| 楼层     | Budget |
| ------ | ------ |
| Floor1 | 10     |
| Floor2 | 20     |
| Floor3 | 35     |
| Floor4 | 50     |
| Floor5 | 70     |

每项调整都会消耗预算：

| 调整    | Cost |
| ----- | ---- |
| 增加盾兵  | 10   |
| 增加突进怪 | 15   |
| 弹药减少  | 20   |
| 回复降低  | 15   |
| 精英怪增加 | 30   |

例如：

Floor3：

Budget：

35。

那么：

可以：

```text
盾兵

10

+

回复减少

15

+

远程怪

10

=

35
```

预算：

耗尽。

停止调整。

这样：

AI 永远不会：

一次：

全部修改。

---

# 7.8 Fairness System（公平性约束）

挑战必须遵循四项原则：

### Principle 1

Always Winnable

任何关卡。

必须：

可通关。

禁止：

无解。

---

### Principle 2

Counter

Not Punishment

AI：

针对。

不是：

惩罚。

例如：

增加盾兵。

同时：

增加近战强化。

玩家：

可以：

改变。

---

### Principle 3

One Change

One Solution

每个挑战。

必须：

存在：

一种：

明显解决方案。

例如：

弹药减少。

↓

近战增强。

玩家：

自然想到：

换Build。

---

### Principle 4

Never Surprise

玩家。

必须：

提前知道。

AI。

改了什么。

因此：

进入下一层前。

必须：

展示：

Director Report。

---

# 7.9 Difficulty Curve（难度曲线）

传统 Roguelike：

```text
难度

↑

一直增长
```

山海镜：

```text
挑战

↑

↓

↑

↓

↑

↓

Boss
```

整体：

呈：

波浪形。

原因：

AI：

会：

根据：

Survival Pressure。

主动：

放缓。

形成：

喘息。

否则。

玩家：

疲劳。

---

# 7.10 Challenge Heat Map（挑战热区）

AI Director 维护一份 **Challenge Heat Map（挑战热区）**，记录玩家近期面对的挑战类型。

例如：

| 挑战类型 | 最近出现次数 |
| ---- | ------ |
| 突进敌人 | 4      |
| 盾兵   | 3      |
| 资源限制 | 1      |
| 技能冷却 | 0      |

当某类挑战连续出现过多时，系统会自动降低其权重，引入其他挑战类型，避免体验单一。

这样可以保证：

* 玩家不会连续三层面对相同套路；
* AI 的针对具有变化，而不是重复；
* 每一局挑战节奏更加丰富。

---

# 7.11 本章总结

Adaptive Challenge Framework 是《山海镜》的**挑战设计中枢（Challenge Brain）**。

它不负责生成敌人，不负责调整数值，而是根据玩家画像生成一份 **Challenge Plan（挑战计划）**，再由 AI Director 将其拆解为敌人配置、规则修改和资源配置。

相比传统 DDA，《山海镜》更强调：

> **不是让游戏变难，而是让挑战变得更聪明。**

通过 **Challenge Budget、Challenge Weight、Fairness System 和 Heat Map** 四个机制，系统既保证了 AI 的针对性，又保证了玩法的公平性、可解释性和长期重复游玩价值。

---

# 第八章 Progression Framework（成长系统）

---

# 8.1 System Vision（系统定位）

Progression（成长）是 Roguelike 的核心驱动力。

传统 Roguelike 的成长方式主要依赖：

* 数值提升
* 装备升级
* 被动强化

随着游戏推进，玩家逐渐变强。

《山海镜》并不追求：

> Build 越来越强。

而是：

> **Build 越来越多样。**

AI Director 会不断限制玩家最依赖的 Build。

因此：

成长目标不是：

最大输出。

而是：

最大适应能力。

---

# 8.2 Growth Philosophy（成长理念）

整个成长体系遵循三个原则。

---

## Principle 01

### Diversity Over Power

玩家获得强化。

不是为了：

越来越强。

而是：

拥有更多选择。

例如：

远程 Build。

很强。

AI：

开始：

增加盾兵。

玩家：

自然：

转近战。

因此：

成长。

来自：

选择。

不是：

数值。

---

## Principle 02

### Counter Creates Evolution

AI：

不会：

削弱玩家。

AI：

创造：

新的问题。

玩家：

寻找：

新的答案。

因此。

Build：

不断进化。

---

## Principle 03

### Every Choice Has Opportunity Cost

每次强化。

都意味着：

放弃。

另一种：

成长路线。

因此：

不存在：

万能 Build。

---

# 8.3 Progression Loop（成长循环）

整个成长循环如下：

```text
进入楼层

↓

完成战斗

↓

获得强化

↓

更新Build

↓

AI重新分析

↓

生成新的挑战

↓

玩家再次成长
```

成长。

永远领先：

AI。

AI。

又追赶：

成长。

形成：

双向演化。

---

# 8.4 Build Architecture（Build架构）

整个 Build 被拆分为四层。

```text
Character

↓

Weapon

↓

Active Skill

↓

Passive
```

四层。

共同组成：

最终 Build。

例如：

```text
方士

↓

猎弓

↓

灵符

↓

暴击流
```

Build。

不是：

某把武器。

而是：

四层组合。

---

# 8.5 Character Layer（角色层）

MVP：

两名角色。

---

## 力士

定位：

近战。

特点：

高生命。

高韧性。

格挡。

爆发。

成长方向：

* 格挡流
* 狂战流
* 反击流

---

## 方士

定位：

远程。

特点：

法术。

控制。

持续输出。

成长方向：

* 风筝流
* 法术流
* 暴击流

角色：

决定：

成长起点。

不是：

成长终点。

---

# 8.6 Weapon Layer（武器层）

武器：

决定：

主要输出方式。

每把武器拥有：

```text
Weapon Type

↓

Attack Pattern

↓

Exclusive Passive
```

例如：

---

猎弓

特点：

高射速。

高暴击。

低范围。

AI标签：

Ranged。

---

干戚

特点：

高伤。

慢速。

霸体。

AI标签：

Heavy。

---

卦镜

特点：

穿透。

法术。

AOE。

AI标签：

Magic。

---

所有武器：

都具有：

成长空间。

没有：

绝对强弱。

---

# 8.7 Active Skill Layer（主动技能）

主动技能：

负责：

改变战斗方式。

MVP：

两格技能槽。

技能：

采用：

Replace。

不是：

无限增加。

原因：

保持：

Build。

简洁。

例如：

技能：

灵盾。

↓

换成：

疾风步。

整个打法：

立即变化。

---

# 8.8 Passive Layer（被动强化）

这是：

整个成长：

最多内容。

被动：

分六类。

---

## Damage

例如：

攻击。

暴击。

元素。

---

## Defense

例如：

护盾。

回血。

减伤。

---

## Mobility

例如：

闪避。

移速。

冲刺。

---

## Utility

例如：

金币。

掉率。

幸运。

---

## Skill

例如：

CD。

技能伤害。

能量。

---

## Special

传奇。

Build。

核心。

整个 Build。

围绕：

Special。

展开。

---

# 8.9 Synergy System（协同系统）

《山海镜》不采用传统：

套装。

而采用：

Build Synergy。

例如：

灵符。

*

暴击。

*

穿透。

↓

形成：

神射流。

AI：

识别：

整个：

Build。

不是：

某件装备。

例如：

```text
Bow

+

Crit

+

Fast

↓

Archetype：

Sniper
```

AI：

针对：

Sniper。

而不是：

Bow。

---

# 8.10 Build Diversity（Build多样性）

系统持续计算：

Build Diversity。

例如：

```text
猎弓

90%

↓

技能

固定

↓

被动

固定

↓

Diversity：

Low
```

AI：

开始：

挑战。

如果：

不断：

更换：

武器。

技能。

Build。

Diversity：

High。

AI：

降低：

针对。

因此：

游戏：

鼓励：

变化。

---

# 8.11 Build Ecology（Build生态系统）

这是《山海镜》的原创机制。

传统 Roguelike：

削弱：

最强 Build。

《山海镜》：

不削。

而是：

改变生态。

例如：

远程。

很多。

↓

AI：

增加：

盾兵。

↓

近战。

变强。

↓

玩家：

转近战。

↓

AI：

增加：

远程怪。

↓

生态。

再次：

变化。

因此：

所有 Build。

始终：

存在。

不会：

淘汰。

形成：

动态平衡。

---

# 8.12 Build Transition（Build转型）

AI：

不仅：

允许。

玩家：

换 Build。

而且：

鼓励。

例如：

第三层。

开始。

玩家：

可以：

免费。

替换：

一个：

Passive。

Boss。

前。

允许：

替换：

武器。

这样。

玩家：

有机会。

重新：

构建。

Build。

整个系统：

支持：

Evolution。

不是：

Reset。

---

# 8.13 Build Recommendation（AI推荐）

AI Director。

不会：

直接。

送强化。

而是：

推荐。

例如：

```text
白泽：

你的近战。

或许。

会更适合。

接下来的挑战。
```

强化。

三选一。

其中。

一项。

更适合。

当前挑战。

玩家。

仍拥有：

选择权。

保证：

AI：

引导。

不是：

控制。

---

# 8.14 Progression Curve（成长曲线）

成长速度：

采用：

前快。

后慢。

模型。

```text
成长

↑

■■■■■■

■■■■

■■

Boss

────────────→

楼层
```

设计目的：

* 第一层快速建立 Build 雏形，给予玩家正反馈。
* 第二至四层通过 AI 持续施压，引导玩家完成 Build 转型。
* 第五层进入 Boss 战前，形成稳定但非固化的 Build。

这样既能保证成长满足感，又避免过早形成无法动摇的最优策略。

---

# 8.15 本章总结

Progression Framework 的目标不是让玩家无限变强，而是构建一个**能够与 AI Director 持续互动的成长系统**。

它通过 **角色、武器、技能、被动、协同、生态、转型** 七个层级，共同组成动态 Build 系统。

与传统 Roguelike 不同，《山海镜》的成长并非追求固定答案，而是在 AI 的不断挑战下，鼓励玩家持续探索新的策略与组合。

最终，**成长的对象不仅是角色，更是玩家自身的决策能力与适应能力。**

---

# 第九章 Combat Framework（战斗系统）

---

# 9.1 Combat Vision（战斗设计目标）

《山海镜》的战斗设计遵循一个核心原则：

> **Easy to Learn，Hard to Master.**

玩家能够：

五分钟学会。

十五分钟熟练。

几十小时仍然可以继续研究新的 Build。

因此。

战斗系统需要同时满足：

* 操作爽快
* Build成长
* AI针对
* 高可读性
* 高反馈

整个战斗体验强调：

> **快节奏、高反馈、低等待、持续决策。**

---

# 9.2 Combat Philosophy（战斗理念）

整个战斗围绕三个核心。

---

## 第一原则

### Action First

动作优先。

任何时候。

玩家输入：

必须立即响应。

包括：

移动。

攻击。

闪避。

技能。

不能出现：

等待。

因此。

输入延迟：

目标：

<50ms。

---

## 第二原则

### Decision Every Second

玩家。

每一秒。

都要：

做决定。

例如：

继续输出？

还是：

闪避？

继续风筝？

还是：

换武器？

因此。

战斗。

不是：

站撸。

而是：

持续判断。

---

## 第三原则

### Feedback Everywhere

任何操作。

必须：

有反馈。

例如：

命中：

停顿（Hit Stop）

暴击：

特殊音效

击杀：

镜面碎裂特效

Rule Modifier：

白泽符文

玩家：

始终知道：

发生了什么。

---

# 9.3 Combat Loop（战斗循环）

战斗采用：

经典ARPG循环。

```text
发现敌人

↓

观察

↓

接近

↓

攻击

↓

闪避

↓

技能

↓

Build联动

↓

击杀

↓

继续推进
```

整个循环：

3~8秒。

持续重复。

---

# 9.4 Combat Layer（三层战斗模型）

战斗拆成三层。

---

## Layer 1

Movement

移动。

包括：

* 行走
* 冲刺
* 闪避
* 位移技能

这是：

整个战斗。

基础。

---

## Layer 2

Attack

攻击。

包括：

普通攻击。

技能。

武器。

Build。

---

## Layer 3

Reaction

反馈。

包括：

受击。

击退。

霸体。

死亡。

整个战斗：

最终体验。

来自：

Reaction。

不是：

Attack。

---

# 9.5 玩家基础能力（Core Actions）

所有角色共享基础动作。

| 动作            | 描述        | 冷却    |
| ------------- | --------- | ----- |
| Move          | 八方向移动     | 无     |
| Light Attack  | 普通攻击      | 无     |
| Heavy Attack  | 重攻击（部分武器） | 武器决定  |
| Dodge         | 闪避        | 0.8 秒 |
| Skill 1       | 主动技能1     | 技能决定  |
| Skill 2       | 主动技能2     | 技能决定  |
| Weapon Switch | 武器切换      | 即时    |
| Interact      | 拾取/开启     | 无     |

设计原则：

所有基础动作均可在移动状态下触发，避免僵硬站桩。

---

# 9.6 武器攻击模型（Weapon Attack Model）

每把武器采用统一模板：

```text
Weapon

↓

Attack Combo

↓

Hit Box

↓

Damage Formula

↓

Feedback
```

例如：

### 猎弓

攻击模式：

连续射击。

特点：

高攻速。

高暴击。

低削韧。

---

### 干戚

攻击模式：

三段重击。

特点：

慢。

高伤。

高霸体。

---

### 卦镜

攻击模式：

法术穿透。

特点：

AOE。

持续伤害。

---

所有武器：

仅：

Attack Pattern。

不同。

整个框架：

统一。

方便：

扩展。

---

# 9.7 Hit Stop（命中停顿）

命中停顿。

决定：

打击感。

设计如下：

| 命中类型   | Hit Stop |
| ------ | -------- |
| 普通攻击   | 0.03 秒   |
| 暴击     | 0.06 秒   |
| 重击     | 0.08 秒   |
| Boss受击 | 0.05 秒   |

Hit Stop：

只暂停：

攻击者。

不是：

整个游戏。

保证：

流畅。

---

# 9.8 Stagger（削韧系统）

敌人拥有：

Poise（韧性）。

例如：

```text
Poise

100

↓

受到重击

↓

70

↓

继续攻击

↓

0

↓

Break
```

Break：

进入：

硬直。

允许：

爆发。

不同敌人：

韧性。

不同。

Boss：

拥有：

两层韧性。

---

# 9.9 Dodge（闪避）

闪避：

不是：

位移。

而是：

生存。

设计：

| 参数    | 数值     |
| ----- | ------ |
| 冷却    | 0.8 秒  |
| 无敌帧   | 0.25 秒 |
| 距离    | 4 米    |
| 可取消攻击 | 是      |

优秀玩家。

依赖：

无敌帧。

不是：

堆血。

---

# 9.10 Skill System（技能系统）

技能采用：

两槽设计。

每个技能：

拥有：

```text
Skill

↓

Type

↓

Cost

↓

Cooldown

↓

Effect

↓

Build Tag
```

例如：

灵盾：

Tag：

Defense。

战吼：

Tag：

Tank。

灵符：

Tag：

Magic。

AI：

读取：

Tag。

不是：

技能名字。

---

# 9.11 Build Interaction（Build联动）

技能。

武器。

被动。

共同组成：

Build。

例如：

```text
猎弓

↓

暴击

↓

穿透

↓

灵符

↓

Sniper Build
```

所有强化：

都通过：

Tag。

连接。

不是：

写死。

---

# 9.12 Enemy Reaction（敌人反馈）

敌人受到攻击后，根据伤害类型产生不同反馈：

| 攻击类型    | 敌人反馈            |
| ------- | --------------- |
| 普通攻击    | 轻微硬直            |
| 重攻击     | 击退 + 削韧         |
| 暴击      | 特殊受击动画 + 镜面裂纹特效 |
| 火焰/持续伤害 | 持续燃烧表现          |
| 控制技能    | 减速、定身或拉拽        |

不同敌人拥有不同的抗性和韧性，不会因为同一种 Build 而产生完全一致的反馈。

---

# 9.13 Combat Camera（战斗镜头）

镜头设计遵循：

**信息优先，表现其次。**

规则如下：

* 普通战斗：固定俯视角，轻微跟随玩家。
* 精英怪登场：镜头轻微拉远，提高战场信息量。
* Boss 战：镜头自动扩大可视范围，保证 Boss 与玩家始终位于屏幕内。
* Director Report：切换至镜界 UI，不打断战斗节奏。

镜头永远服务于可读性，不追求复杂运镜。

---

# 9.14 Combat Feel Checklist（战斗手感清单）

为了保证最终体验，所有战斗动作均需通过以下检查：

| 检查项      | 目标                 |
| -------- | ------------------ |
| 输入响应     | ≤50ms              |
| 命中反馈     | Hit Stop + 音效 + 特效 |
| 技能可读性    | 一眼识别范围与效果          |
| 闪避公平性    | 无敌帧稳定可预期           |
| 敌人反馈     | 每种攻击都有差异           |
| Build 联动 | 强化能明显改变玩法          |

这份 Checklist 将作为后续开发与测试阶段的重要验收标准。

---

# 9.15 本章总结

Combat Framework 是《山海镜》的**即时体验层（Moment-to-Moment Gameplay）**。

它确保：

* 玩家每一次输入都有即时响应；
* 每一次命中都有明确反馈；
* 每一次 Build 成长都能改变战斗方式；
* 每一次 AI 调整都能通过战斗体现出来。

最终形成：

> **Action（动作） × Build（成长） × AI Director（动态挑战）**

三者相互耦合的核心玩法。

---

# 第十章 Enemy Framework（敌人系统）

---

# 10.1 System Vision（系统定位）

Enemy Framework 是整个战斗系统的挑战载体。

敌人的目标不是：

击杀玩家。

而是：

> **迫使玩家改变策略。**

因此。

《山海镜》的敌人遵循：

Behavior First。

不是：

Stat First。

换句话说。

敌人的区别来自：

行为。

而不是：

攻击力。

---

# 10.2 Enemy Design Philosophy（设计理念）

整个敌人设计遵循四条原则。

---

## Principle 01

Counter Build

敌人存在的意义。

不是：

增加数量。

而是：

克制某类 Build。

例如：

盾兵。

存在意义。

不是：

防御高。

而是：

针对远程。

---

## Principle 02

Clear Readability

玩家。

一眼。

就知道：

敌人会干什么。

例如：

盾兵。

举盾。

↓

即将防御。

穷奇。

低头。

↓

即将冲锋。

所有攻击。

必须：

提前预警。

---

## Principle 03

One Enemy

One Purpose

每种敌人。

只负责：

一种玩法。

例如：

Tank。

负责：

推进。

Rush。

负责：

冲脸。

Support。

负责：

治疗。

避免：

一个敌人。

什么都会。

---

## Principle 04

AI Controllable

所有敌人。

都必须：

能够：

被 AI Director。

动态组合。

因此。

AI。

永远控制：

Enemy Tag。

不是：

Enemy Name。

---

# 10.3 Enemy Architecture（敌人架构）

所有敌人：

采用：

统一模板。

```text
Enemy

↓

Archetype

↓

Behavior Tree

↓

Ability

↓

Tag

↓

Spawn Weight
```

任何新敌人。

只需要：

填写。

配置。

无需：

修改。

程序。

---

# 10.4 Enemy Archetype（敌人类型）

整个游戏。

所有敌人。

统一分七类。

---

## Type 01

Grunt（杂兵）

职责：

基础压力。

特点：

数量多。

血量低。

行为：

简单。

代表：

魑魅。

山魈。

---

## Type 02

Tank（盾兵）

职责：

限制远程。

特点：

举盾。

推进。

保护队友。

代表：

玄龟。

石敢当。

---

## Type 03

Rush（突进）

职责：

冲散阵型。

特点：

高速。

扑击。

贴脸。

代表：

穷奇。

蛊雕。

---

## Type 04

Shooter（射手）

职责：

远程压制。

特点：

保持距离。

不断射击。

代表：

鸣蛇。

朱厌。

---

## Type 05

Controller（控制）

职责：

限制移动。

特点：

减速。

束缚。

冰冻。

代表：

冰夷。

藤妖。

---

## Type 06

Support（辅助）

职责：

强化敌军。

特点：

回血。

Buff。

召唤。

代表：

九尾狐。

英招。

---

## Type 07

Elite（精英）

职责：

战斗核心。

特点：

拥有：

多个技能。

多个阶段。

代表：

毕方。

狰。

相柳。

---

# 10.5 Enemy AI（敌人AI）

所有敌人：

共享：

统一状态机。

```text
Idle

↓

Patrol

↓

Detect

↓

Chase

↓

Attack

↓

Recover

↓

Attack

↓

Dead
```

不同敌人。

区别：

Attack。

不同。

---

# 10.6 Behavior Tree（行为树）

统一行为树。

```text
是否发现玩家？

↓

否

↓

巡逻

↓

是

↓

距离判断

↓

近

↓

攻击

↓

远

↓

追击

↓

攻击结束

↓

重新判断
```

所有敌人：

继承：

统一BT。

只修改：

Attack Node。

---

# 10.7 Enemy Tag System（标签系统）

这是。

整个AI Director。

真正控制。

敌人的方式。

例如：

穷奇。

不是：

穷奇。

而是：

```text
Rush

Fast

Light

Melee
```

玄龟：

```text
Tank

Shield

Slow

Front
```

毕方：

```text
Elite

Fire

AOE

Magic
```

Director。

组合：

Tag。

不是：

怪物。

---

# 10.8 Spawn System（生成系统）

Director。

不会：

说：

生成：

穷奇。

而是：

输出：

```json
{
"Rush":35,

"Tank":25,

"Shooter":40
}
```

Generator。

根据：

权重。

随机。

选择：

对应敌人。

例如：

Rush：

池子：

穷奇。

蛊雕。

天狗。

随机。

一个。

保证：

重复游玩。

---

# 10.9 Threat Value（威胁值）

每种敌人：

拥有：

Threat。

例如：

| 敌人         | Threat |
| ---------- | ------ |
| 杂兵         | 5      |
| Rush       | 15     |
| Tank       | 20     |
| Shooter    | 18     |
| Controller | 25     |
| Support    | 30     |
| Elite      | 60     |

每个房间。

拥有：

Threat Budget。

例如：

40。

生成器：

自动。

计算。

例如：

```text
Tank

20

+

Rush

15

+

Grunt

5

=

40
```

房间。

完成。

---

# 10.10 Enemy Synergy（敌人协同）

敌人。

不是：

独立。

而是：

组合。

例如：

Tank。

保护。

Shooter。

Rush。

逼近。

玩家。

Controller。

减速。

玩家。

形成：

组合。

整个战斗。

更加：

丰富。

---

# 10.11 Director Counter Matrix（AI克制矩阵）

AI Director 根据玩家 Build 调整敌人标签，而不是指定具体怪物。

| 玩家 Build | 优先提高标签             | 设计目的      |
| -------- | ------------------ | --------- |
| 高远程输出    | Tank、Rush          | 压缩输出空间    |
| 高近战输出    | Shooter、Controller | 拉开距离、限制接近 |
| 高爆发技能    | Elite、Disrupt（后续）  | 打断节奏、延长战斗 |
| 高回复流     | Burst、Debuff       | 增加瞬时压力    |
| 高频闪避     | Area、Trap（后续）      | 控制走位空间    |

这种矩阵化设计便于后续扩展新的敌人与新的 Build，而无需修改 Director 的逻辑。

---

# 10.12 Boss 与普通敌人的区别

Boss 不属于 Enemy Framework 的普通敌人体系。

Boss 采用独立框架，但仍继承：

* Threat System
* Tag System
* Behavior Framework

额外增加：

* 多阶段（Phase）
* 技能池（Skill Pool）
* AI 动态技能
* 战场机制（Arena Mechanics）

这样既保持统一架构，又能满足 Boss 的独特体验。

---

# 10.13 本章总结

Enemy Framework 是《山海镜》的**挑战执行层（Challenge Execution Layer）**。

AI Director 不直接"生成怪物"，而是通过：

* **Enemy Tag**
* **Threat Budget**
* **Spawn Weight**
* **Counter Matrix**

四套系统动态构建敌人组合。

这种设计具有三个优势：

1. **高度可扩展**：新增异兽只需配置标签与行为，无需修改 AI Director。
2. **高度可维护**：策划可通过配置表调整敌人权重和组合策略。
3. **高度可复用**：普通敌人、精英敌人甚至 Boss 都能共享同一套底层框架。

---

# 第十一章 Encounter Framework（遭遇系统）

---

# 11.1 System Vision（系统定位）

Encounter（遭遇）决定：

玩家每一分钟。

会遇见什么。

Enemy Framework。

负责：

怪物。

Encounter。

负责：

怪物如何出现。

因此。

Enemy：

是：

角色。

Encounter：

才是：

导演。

AI Director。

真正控制的是：

Encounter。

不是：

Enemy。

---

# 11.2 Encounter Philosophy（设计理念）

整个遭遇设计遵循四条原则。

---

## Principle 01

Challenge Before Damage

挑战。

来自：

局势。

不是：

伤害。

例如：

三个盾兵。

不一定危险。

一个盾兵。

配两个Rush。

危险。

因此。

Encounter。

设计的是：

组合。

---

## Principle 02

Every Encounter Tells A Story

每场战斗。

都应该：

像一个：

小故事。

例如：

第一波。

杂兵。

↓

第二波。

盾兵推进。

↓

第三波。

Rush偷袭。

整个战斗。

有：

节奏。

不是：

一起刷。

---

## Principle 03

Pressure Wave

压力。

必须：

波浪式。

不能：

一直最高。

例如：

```text
■■■■

■■

■■■■■

■

Boss
```

玩家：

需要：

喘息。

---

## Principle 04

AI Directed

AI。

控制：

整场遭遇。

不是：

控制：

单只敌人。

---

# 11.3 Encounter Structure（遭遇结构）

每场战斗：

统一采用：

四阶段。

```text
Spawn

↓

Pressure

↓

Peak

↓

Recovery
```

---

## Phase 01

Spawn

敌人。

进入。

玩家：

观察。

布局。

---

## Phase 02

Pressure

敌人：

开始：

形成阵型。

Tank。

推进。

Rush。

绕后。

Shooter。

压制。

---

## Phase 03

Peak

整场战斗。

最高压力。

Elite。

出现。

技能。

同时释放。

玩家：

必须：

改变。

---

## Phase 04

Recovery

剩余：

少量敌人。

给予：

喘息。

进入：

奖励。

---

# 11.4 Encounter Types（遭遇类型）

整个游戏。

所有房间。

统一分六类。

---

Combat

普通战斗。

占比：

60%。

---

Elite

精英战。

高奖励。

高风险。

---

Challenge

特殊挑战。

例如：

限时。

无回复。

生存。

---

Puzzle

机关。

后续。

开放。

---

Story

剧情。

白泽。

对话。

---

Boss

最终。

战斗。

---

AI Director。

控制：

比例。

---

# 11.5 Threat Budget（遭遇预算）

Enemy。

拥有：

Threat。

Encounter。

拥有：

Budget。

例如：

第三层。

Budget：

120。

生成：

```text
Tank

20

Rush

15

Shooter

15

Elite

60

Grunt

10

=

120
```

因此。

不同楼层。

压力：

自然：

增长。

---

# 11.6 Wave Generator（波次生成）

AI。

不会：

一次：

刷怪。

采用：

Wave。

例如：

```text
Wave1

Grunt

↓

Wave2

Tank

↓

Wave3

Rush

↓

Wave4

Elite
```

形成：

节奏。

---

# 11.7 Spawn Rules（刷怪规则）

刷怪遵循以下规则：

* 玩家出生点周围一定范围内禁止生成敌人，保证开局安全。
* 新敌人不会直接刷新在玩家视野中心，避免"贴脸生成"。
* 同一时间屏幕内高威胁单位数量设定上限。
* 精英敌人与普通敌人之间保留支援关系，避免单独出现造成体验割裂。

所有规则都由 Encounter Manager 统一管理，而不是由具体敌人控制。

---

# 11.8 Dynamic Encounter（动态遭遇）

AI Director 可以在楼层开始前调整 Encounter 的整体风格。

例如：

| Encounter 风格 | 特征           |
| ------------ | ------------ |
| Assault（强攻）  | 高密度近战推进      |
| Siege（围攻）    | 盾兵 + 射手阵型    |
| Hunt（狩猎）     | 多个 Rush 分散包夹 |
| Survival（生存） | 持续刷怪，坚持一定时间  |
| Duel（决斗）     | 少量高强度敌人      |

同一套敌人，在不同 Encounter 风格下，会产生完全不同的战斗体验。

---

# 11.9 Arena Design（战场设计）

Encounter 不仅决定敌人，也决定战场。

AI Director 可调节：

* 掩体数量
* 可破坏物件
* 狭窄或开阔区域
* 陷阱启用状态
* 环境机关

例如：

远程玩家：

增加：

掩体。

减少：

开阔地。

近战玩家：

增加：

空旷区域。

方便：

走位。

因此。

Arena。

也是：

Challenge。

---

# 11.10 Elite Encounter（精英遭遇）

Elite。

不是：

血量更多。

而是：

改变：

整场战斗。

例如：

毕方。

出现。

↓

所有敌人。

获得：

燃烧。

九尾狐。

出现。

↓

所有敌人。

回血。

Elite。

改变：

规则。

不是：

数值。

---

# 11.11 Boss Encounter（Boss 遭遇）

Boss 战采用固定三阶段结构：

```text
Phase1

↓

Transition

↓

Phase2

↓

Transition

↓

Phase3
```

AI Director 仅在 Boss 战开始时，根据玩家画像选择：

* 一个动态技能；
* 一个战场机制；
* 一个环境修正。

Boss 战过程中不再实时修改，确保公平性与可读性。

---

# 11.12 Encounter Telemetry（遭遇数据）

每场 Encounter 结束后记录：

| 数据     | 用途             |
| ------ | -------------- |
| 战斗时长   | 调整节奏           |
| 玩家受击次数 | 压力评估           |
| 技能使用   | Build 分析       |
| 闪避成功率  | 操作分析           |
| 敌人击杀顺序 | 战术分析           |
| 资源消耗   | Resource Model |

这些数据会反馈给 Player Modeling，形成下一层 AI Director 的输入。

---

# 11.13 本章总结

Encounter Framework 是《山海镜》的**节奏控制层（Pacing Layer）**。

它连接：

* Enemy Framework（敌人）
* Arena（战场）
* AI Director（导演）
* Player Modeling（玩家建模）

通过 **Threat Budget、Wave Generator、Encounter Style、Arena Control** 四套机制，让每一场战斗都具有明确的节奏、主题和策略变化。

玩家面对的不再是一群随机刷新的敌人，而是一场由 AI Director 精心编排的"动态试炼"。



# 第十二章 Mirror Cognitive Architecture（镜界认知架构）

---

# 12.1 为什么需要认知层（Why Cognition）

传统AI Director：

看到：

玩家远程。

↓

增加盾兵。

结束。

但是。

真正的人类不会这样。

真正的人类会想：

> **他为什么一直玩远程？**

可能：

他喜欢。

也可能：

只是这局没拿到近战。

也可能：

他故意骗AI。

所以。

AI应该：

推理。

不是：

统计。

---

# 12.2 Mirror Mind（镜界意识）

整个AI。

被拆成：

五层。

```text
Perception

↓

Memory

↓

Inference

↓

Planning

↓

Reflection
```

这五层。

组成：

Mirror Mind。

---

## 第一层

Perception

观察。

负责：

记录。

所有行为。

例如：

攻击。

走位。

Build。

受伤。

这里只：

采集。

---

## 第二层

Memory

传统Director。

没有：

记忆。

Mirror。

有。

记忆。

又分：

### Short Memory

最近：

两层。

例如：

```text
Layer3

Bow

↓

Layer4

Bow
```

### Long Memory

最近：

十局。

例如：

```text
玩家

70%

喜欢：

远程
```

AI开始：

认识：

玩家。

---

## 第三层

Inference

这是。

真正AI。

例如：

行为：

```text
一直远程
```

AI不会：

立即。

认为：

远程流。

而是：

计算：

```text
Confidence

65%
```

说明：

还不能确定。

下一层：

继续观察。

如果：

连续：

四层。

都是：

远程。

Confidence：

95%。

AI：

真正开始：

针对。

这样。

不会：

误判。

---

# 12.3 Strategy Confidence（策略置信度）

整个系统：

最重要。

例如：

```text
Bow

95%
```

说明：

远程。

成立。

如果：

```text
Bow

52%
```

说明：

玩家：

可能：

只是：

临时。

拿到。

因此。

AI。

不会：

立刻。

改规则。

---

# 12.4 Intent Inference（意图推理）

这是目前所有游戏都几乎没有做的。

AI开始：

猜。

玩家。

目的。

例如：

```text
玩家

突然

换近战。
```

AI：

思考：

可能：

①

真的换Build。

②

骗AI。

③

试试玩。

因此。

AI。

保持：

观察。

而不是：

立即。

调整。

---

# 12.5 Reflection（反思）

真正高级AI。

会：

反思。

例如：

上一层。

AI：

增加盾兵。

结果。

玩家：

打得。

更轻松。

AI：

发现：

策略。

失败。

于是：

修改。

下一层。

---

# 12.6 Director Learning（导演学习）

Director。

不仅：

学习。

玩家。

也学习：

自己。

例如：

```text
策略A

成功率

80%
```

继续。

使用。

```text
策略B

成功率

20%
```

降低。

权重。

这就是：

真正：

Learning。

不是：

Prompt。

---

# 12.7 Cognitive Loop（认知闭环）

整个AI。

变成：

```text
Observe

↓

Remember

↓

Understand

↓

Predict

↓

Plan

↓

Challenge

↓

Observe
```

注意。

这里。

不是：

Analyze。

而是：

Understand。

整个等级。

提高。

---

# 12.8 Mirror Personality（镜界人格）

AI。

不是：

只有：

一个。

人格。

而是：

拥有：

人格参数。

例如：

```text
Curiosity

85

Aggression

30

Fairness

90

Creativity

70
```

以后：

温和。

正常。

整人。

全部：

只是：

人格参数。

变化。

程序：

完全。

不用。

改。

---

# 12.9 为什么这是整个GDD最大的创新

如果只是：

AI Director。

腾讯。

可能：

觉得：

不错。

但是。

如果：

加入：

Mirror Cognitive Architecture。

那么。

整个项目。

就从：

> **AI控制游戏**

升级成：

> **AI理解玩家，再设计游戏。**

这两句话。

差距。

非常大。

因为。

前者：

属于：

AI Tool。

后者：

属于：

AI Native。

也是目前学术界和工业界正在探索的方向。

---

# 第十三章 Rule Modifier Framework（规则演化系统）

---

# 13.1 System Vision（系统定位）

Rule Modifier 是《山海镜》最重要的玩法系统。

传统 Roguelike 中：

AI 修改：

敌人。

《山海镜》中：

AI 修改：

游戏规则。

因此。

整个游戏真正变化的是：

游戏世界。

不是：

怪物属性。

玩家真正需要适应的是：

新的规则。

而不是：

新的数值。

因此：

Rule Modifier 是：

整个 AI Director 的执行层。

---

# 13.2 Design Philosophy（设计理念）

整个 Rule Framework 遵循四项原则。

---

## Principle 01

Rules Change Gameplay

规则。

必须改变：

玩法。

而不是：

数值。

例如：

优秀。

```text
回复药效果降低30%
```

因为：

玩家。

打法。

改变。

---

错误。

```text
敌人HP+30%
```

玩家。

打法。

没有变化。

只是：

打得更久。

---

## Principle 02

Every Rule Creates Choice

任何规则。

都必须：

存在：

至少：

一种。

解决方案。

例如：

弹药减少。

↓

近战增强。

玩家：

自然。

换Build。

因此。

Rule。

不是：

限制。

而是：

选择。

---

## Principle 03

Rules Are Temporary

所有规则。

仅持续：

当前楼层。

下一层。

AI。

重新设计。

避免：

永久Debuff。

造成：

挫败感。

---

## Principle 04

Visible Before Active

规则。

必须：

提前。

展示。

玩家。

进入关卡前。

已经知道：

这一层。

发生：

什么。

---

# 13.3 Rule Layer（规则层）

整个规则：

统一分五层。

```text
Combat

↓

Resource

↓

Environment

↓

Enemy

↓

Special
```

所有规则。

都属于：

其中一层。

---

# 13.4 Combat Rules（战斗规则）

影响：

玩家。

战斗。

例如：

| Rule     | 示例        |
| -------- | --------- |
| Damage   | 近战伤害+20%  |
| Critical | 暴击率-15%   |
| Cooldown | 技能CD+1秒   |
| Dodge    | 闪避冷却+0.3秒 |
| Heal     | 回复效率-25%  |

注意。

不会：

直接。

改：

Boss。

只改：

规则。

---

# 13.5 Resource Rules（资源规则）

影响：

资源。

例如：

| Rule   | 示例       |
| ------ | -------- |
| Ammo   | 弹药减少20%  |
| Heal   | 回复掉率减少   |
| Gold   | 金币增加     |
| Chest  | 宝箱品质下降   |
| Energy | 能量恢复速度下降 |

资源。

影响：

Build。

不是：

战斗。

---

# 13.6 Environment Rules（环境规则）

这是正式版重点。

MVP：

可以。

保留。

框架。

例如：

雾天：

远程距离。

减少。

夜晚：

视野。

下降。

暴雨：

火属性。

削弱。

强风：

投射物。

偏移。

这些。

全部：

属于：

Rule。

不是：

地图。

---

# 13.7 Enemy Rules（敌人规则）

AI。

不仅：

改：

敌人数量。

还可以：

改：

敌人行为。

例如：

```text
Rush速度

+20%
```

```text
Tank护盾恢复
```

```text
Shooter

攻击频率增加
```

注意。

不是：

改：

攻击力。

而是：

行为。

---

# 13.8 Special Rules（特殊规则）

这是。

传奇规则。

例如：

### 镜像世界

玩家。

伤害。

提高。

敌人。

也提高。

---

### 猎杀时刻

Elite。

全部。

出现。

奖励。

翻倍。

---

### 神明试炼

禁止。

回血。

Boss。

掉：

传奇强化。

这些规则：

高风险。

高收益。

形成：

Roguelike高潮。

---

# 13.9 Rule Tag（规则标签）

每条规则：

拥有：

Tag。

例如：

```text
Combat

Positive

Risk

Melee
```

或者：

```text
Resource

Negative

Ammo

Ranged
```

AI Director。

读取：

Tag。

不是：

Rule名字。

方便：

扩展。

---

# 13.10 Rule Cost（规则预算）

每条规则。

拥有：

Cost。

例如：

| Rule        | Cost |
| ----------- | ---- |
| CD+10%      | 10   |
| Heal-20%    | 20   |
| Ammo-30%    | 20   |
| Elite+1     | 35   |
| Mirror Rule | 50   |

AI。

拥有：

Rule Budget。

例如：

Floor3：

Budget：

40。

可以：

```text
Heal

20

+

CD

10

+

Rush

10

=

40
```

预算。

结束。

---

# 13.11 Rule Conflict（规则冲突）

所有规则。

必须：

经过：

Conflict Check。

例如：

禁止：

```text
回血减少

+

无回血
```

禁止：

```text
弹药减少

+

远程伤害减少
```

因为：

玩家。

无解。

系统。

自动。

过滤。

---

# 13.12 Rule Synergy（规则联动）

规则之间。

也可以：

组合。

例如：

### 贫瘠之地

金币。

增加。

↓

回复。

减少。

形成：

资源管理。

挑战。

---

### 狂风暴雨

风。

*

雨。

↓

远程。

下降。

↓

近战。

增强。

规则：

组合。

比。

单条。

更丰富。

---

# 13.13 Rule 生命周期（Lifecycle）

每条规则都遵循统一生命周期：

```text
生成

↓

预告

↓

生效

↓

运行

↓

结算

↓

失效

↓

归档
```

在规则生效期间，系统会持续记录玩家是否成功适应该规则，并将结果反馈给 Mirror Cognitive Architecture。

例如：

* 玩家是否主动更换 Build；
* 是否改变战斗方式；
* 是否在规则下保持高效率。

这部分数据将影响 AI 后续的策略判断。

---

# 13.14 Rule Telemetry（规则统计）

每条规则都记录运行效果：

| 数据     | 作用            |
| ------ | ------------- |
| 触发次数   | 使用频率分析        |
| 玩家适应率  | 判断规则是否有效      |
| 玩家放弃率  | 是否导致挫败        |
| 平均战斗时间 | 节奏评估          |
| AI 成功率 | Director 学习输入 |

通过这些统计，AI 可以逐渐降低效果差、体验差的规则权重，提高真正能够促使玩家改变策略的规则出现概率。

---

# 13.15 本章总结

Rule Modifier Framework 是《山海镜》的**玩法演化引擎（Gameplay Evolution Engine）**。

它将传统 Roguelike 的"数值变化"升级为"规则变化"，通过 **Combat、Resource、Environment、Enemy、Special** 五大规则层，构建出一个能够被 AI Director 动态组合、持续演化的规则生态。

最终，玩家面对的不再只是越来越强的敌人，而是一个会不断改变游戏规则、迫使自己重新思考策略的世界。

---

# 第十四章 Boss Framework（Boss系统）

---

# 14.1 System Vision（系统定位）

Boss 是一局游戏的最终考试。

普通敌人：

考察：

操作。

Encounter：

考察：

节奏。

AI Director：

考察：

适应能力。

Boss：

考察：

玩家是否真正完成了：

**Build Evolution（Build进化）。**

Boss 不应该：

只是：

血更厚。

伤害更高。

Boss 应该：

验证：

玩家是否学会了新的玩法。

---

# 14.2 Boss Design Philosophy（Boss设计理念）

整个Boss设计遵循五项原则。

---

## Principle 01

Boss Tests Learning

Boss。

不是：

提高难度。

Boss。

测试：

玩家。

是否真正：

学会：

改变。

例如：

玩家：

一直远程。

Boss：

拥有：

反远程机制。

玩家：

如果：

没有：

改变。

Boss。

非常困难。

如果：

改变。

Boss。

恢复：

正常。

---

## Principle 02

Boss Is Predictable

Boss。

不能：

随机秒人。

所有技能。

必须：

拥有：

前摇。

音效。

特效。

可学习。

---

## Principle 03

Every Death Teaches

玩家：

死亡。

必须：

知道：

为什么。

例如：

白泽：

> 你仍然依赖远程攻击。

> 烛龙已经完全识破你的策略。

因此：

失败。

具有：

学习价值。

---

## Principle 04

AI Assists

Not Controls

Boss。

战斗开始。

AI。

停止。

动态调整。

避免：

作弊。

Boss。

进入。

固定模式。

保证：

公平。

---

## Principle 05

Boss Reflects Player

Boss。

实际上：

就是：

玩家。

镜像。

Boss。

会：

针对：

玩家。

Build。

形成：

最终试炼。

---

# 14.3 Boss Architecture（Boss架构）

所有Boss。

采用：

统一模板。

```text
Boss

↓

Phase

↓

Skill Pool

↓

AI Skill

↓

Arena Rule

↓

Reward
```

因此。

以后：

新增Boss。

只需要：

配置。

即可。

---

# 14.4 Boss State Machine（Boss状态机）

所有Boss：

统一状态。

```text
Idle

↓

Intro

↓

Phase1

↓

Transition

↓

Phase2

↓

Transition

↓

Phase3

↓

Death
```

Boss。

不采用：

无限循环。

而采用：

阶段。

推进。

---

# 14.5 Three Phase Design（三阶段设计）

所有Boss。

统一：

三阶段。

---

## 第一阶段

Observation（观察）

Boss。

测试：

玩家。

打法。

技能：

简单。

给予：

学习机会。

---

## 第二阶段

Counter（反制）

Boss。

开始：

针对。

玩家。

例如：

远程。

↓

增加。

冲锋。

近战。

↓

增加。

AOE。

---

## 第三阶段

Evolution（最终）

Boss。

开启：

最终技能。

Arena。

发生变化。

玩家。

必须：

真正。

掌握。

Build。

---

# 14.6 Boss Skill Pool（技能池）

Boss。

技能。

分五类。

---

Basic

基础攻击。

---

Movement

位移。

---

Area

范围攻击。

---

Counter

克制。

Build。

---

Ultimate

终极技能。

AI。

只选择：

Counter。

其它。

固定。

---

# 14.7 Dynamic Boss Skill（动态技能）

Boss。

拥有：

一格。

AI技能。

例如：

玩家：

远程。

↓

Boss：

获得：

Mirror Dash。

快速。

突进。

玩家：

近战。

↓

Boss：

获得：

Fire Ring。

持续：

AOE。

整个Boss。

每局。

略有：

不同。

---

# 14.8 Arena Modifier（战场机制）

Boss 战不仅由 Boss 本体决定，还由战场机制共同构成。

AI Director 可在战斗开始前，从机制池中选择一项 Arena Modifier：

| Arena Rule     | 效果        |
| -------------- | --------- |
| Mirror Fog     | 周期性降低视野   |
| Burning Ground | 地面持续燃烧    |
| Falling Rocks  | 随机区域落石    |
| Mirror Crystal | 击碎后获得临时增益 |
| Wind Current   | 投射物轨迹偏移   |

Arena Modifier 在战斗开始前通过白泽进行提示，玩家能够提前制定策略。

---

# 14.9 Boss Memory（Boss记忆）

Boss 会读取 Mirror Cognitive Architecture 的输出，但不会实时学习。

读取内容包括：

* 玩家 Archetype（类型）
* Build Tag（Build标签）
* Adaptability（适应能力）
* 最近两层 Rule 适应情况

例如：

```text
Archetype：

Ranger

↓

Boss：

优先选择：

Mirror Dash
```

Boss 战开始后，这些数据固定，不再更新。

---

# 14.10 Reward Design（奖励设计）

Boss 奖励强调：

成长。

不是：

数值。

奖励池包括：

| 奖励                | 描述            |
| ----------------- | ------------- |
| Legendary Passive | 传奇强化          |
| Mirror Fragment   | 解锁白泽日志与世界设定   |
| Character Memory  | 新角色背景故事       |
| Director Record   | 本局 AI 导演分析报告  |
| Cosmetic          | 外观、特效、称号（正式版） |

Boss 不掉落普通装备，确保奖励具有里程碑意义。

---

# 14.11 First Boss：烛龙（MVP）

| 项目         | 内容    |
| ---------- | ----- |
| 名称         | 烛龙    |
| 出处         | 《山海经》 |
| 定位         | 镜界守门者 |
| 核心机制       | 昼夜切换  |
| AI 动态技能    | 1 个   |
| Phase 数量   | 3     |
| Arena Rule | 镜面裂隙  |

### Phase 1：昼

* 光照充足；
* 远程攻击容易命中；
* Boss 以基础攻击为主。

### Phase 2：昼夜交替

* 场景明暗交替；
* 镜面裂隙出现；
* Boss 获得 AI 动态技能。

### Phase 3：永夜

* 视野缩小；
* Arena Rule 全面生效；
* Boss 使用 Ultimate 技能。

设计目的不是提高伤害，而是逐步增加信息处理压力。

---

# 14.12 Boss Evaluation（Boss评价系统）

Boss 战结束后，白泽不会只评价输赢，而是评价玩家的成长。

例如：

> **镜界评价：**
> 你在第二阶段成功改变了攻击节奏，因此烛龙失去了对你的压制。你的适应能力达到 **S**，但资源管理仍有不足。

如果玩家失败：

> **镜界评价：**
> 你识别了昼夜机制，但仍过于依赖远程输出。下次尝试利用近战窗口，或许会有不同的结果。

评价始终围绕**学习与适应**，而不是单纯批评失败。

---

# 14.13 本章总结

Boss Framework 是《山海镜》的**能力验证层（Capability Validation Layer）**。

Boss 不只是最终敌人，更是 AI Director 为玩家设计的"毕业考试"。

它通过：

* **三阶段结构（Three Phase）**
* **动态技能（Dynamic Skill）**
* **Arena Modifier**
* **Boss Memory**
* **成长评价（Boss Evaluation）**

将玩家整局积累的 Build、操作与策略适应能力集中检验。

最终，Boss 战不只是击败一个神话生物，而是完成一次对自身策略与成长的验证。

---

# 第十五章 AI Transparency Framework（AI透明反馈系统）

---

# 15.1 System Vision（系统定位）

AI Transparency Framework（ATF）负责建立玩家与 AI Director 之间的沟通机制。

它回答一个核心问题：

> **AI 为什么这样设计这一层？**

如果玩家不知道：

AI 为什么修改规则。

那么：

所有 AI 设计都会被误认为：

随机。

作弊。

Bug。

因此。

Explainability（可解释性）

就是：

整个 AI Native 的基础。

---

# 15.2 Design Philosophy（设计理念）

透明反馈遵循五项原则。

---

## Principle 01

Explain Before Challenge

AI。

先解释。

后挑战。

例如：

进入下一层。

先看到：

导演报告。

再进入：

战斗。

而不是：

打完才知道。

---

## Principle 02

Never Hide Director

白泽。

永远存在。

玩家：

任何时候。

都知道：

是谁。

设计：

这一层。

Director：

不是：

后台程序。

而是：

世界的一部分。

---

## Principle 03

Show Reason

Not Result

不要：

告诉玩家。

```text
盾兵+30%
```

应该：

告诉玩家。

```text
因为：

你连续三层。

依赖远程攻击。

所以。

盾兵增加。
```

玩家。

理解：

原因。

不是：

结果。

---

## Principle 04

Explain Strategy

Not Formula

玩家。

不需要知道：

```text
Build Score

87
```

玩家。

需要知道：

```text
你已经形成稳定远程打法。
```

解释：

策略。

不是：

数学。

---

## Principle 05

Feedback Creates Trust

解释越充分。

玩家：

越相信：

AI。

真正：

思考过。

而不是：

作弊。

---

# 15.3 Explain Pipeline（解释流程）

整个解释流程如下：

```text
Player Model

↓

Mirror Mind

↓

Director Decision

↓

Reason Generator

↓

Narrative Generator

↓

WhiteZe Report

↓

Player
```

AI。

不会：

直接。

输出。

JSON。

而是：

先生成：

解释。

---

# 15.4 Director Report（导演报告）

这是。

玩家。

每层。

都会看到。

UI。

例如：

```
━━━━━━━━━━━━━━━━━━━

镜界观察报告

━━━━━━━━━━━━━━━━━━━

观察对象：

试炼者

行为分析：

✓ 连续使用猎弓

✓ 很少切换技能

✓ 闪避成功率高

镜界判断：

你已经形成稳定远程打法。

下一层调整：

● 盾兵增加

● 突进敌人增加

● 回复保持正常

白泽：

真正的猎人，

不会永远站在安全的位置。

━━━━━━━━━━━━━━━━━━━
```

Director Report：

固定。

每层。

展示。

---

# 15.5 Mirror Insight（镜界洞察）

Director Report。

只展示：

结果。

Mirror Insight。

展示：

AI。

真正看到：

什么。

例如：

```
本层观察：

远程攻击：

92%

近战攻击：

8%

Build变化：

低

策略多样性：

★

适应能力：

★★★☆

```

玩家。

知道：

AI。

为什么。

这样。

想。

---

# 15.6 WhiteZe Commentary（白泽评论）

白泽。

不会：

固定。

一句话。

采用：

模板。

例如：

观察：

Build。

↓

生成：

对白。

例如：

Build集中：

> 你的武器已经成为你的习惯。

资源很多：

> 你似乎还没有真正陷入困境。

打法变化：

> 很好，你终于开始改变了。

整个对白：

参数化。

生成。

---

# 15.7 Visual Language（视觉语言）

所有 AI 行为都有统一视觉表现。

| AI 行为       | 视觉反馈     |
| ----------- | -------- |
| Rule 修改     | 镜面裂纹扩散   |
| Enemy 调整    | 白色符文流动   |
| Resource 修改 | 地面浮现镜纹   |
| Boss Skill  | 镜界符文展开   |
| Arena Rule  | 天空出现镜界裂隙 |

玩家。

不用：

看文字。

也知道：

AI。

介入。

---

# 15.8 Mirror Timeline（镜界时间线）

正式版加入：

Timeline。

记录：

整个Run。

AI。

每一次：

修改。

例如：

```
Floor1

建立画像

↓

Floor2

增加Rush

↓

Floor3

修改规则

↓

Floor4

玩家改变Build

↓

Floor5

Boss获得Mirror Dash
```

玩家。

结束。

可以：

完整复盘。

---

# 15.9 AI Confidence Display（AI置信度）

Mirror Mind。

拥有：

Confidence。

玩家。

可以：

看到。

例如：

```
AI判断：

远程玩家

Confidence

81%
```

如果：

Confidence。

下降。

例如：

```
52%
```

说明：

AI。

开始：

怀疑。

玩家。

改变了。

这个设计。

非常新颖。

---

# 15.10 Explainable Failure（失败解释）

死亡。

不是：

Game Over。

而是：

Learning Report。

例如：

```
死亡原因：

连续三次。

站位过近。

镜界建议：

尝试利用翻滚。

保持距离。

白泽：

我不是击败了你。

是你拒绝了改变。
```

这样。

失败。

也成为：

成长。

---

# 15.11 Director Archive（导演档案）

正式版新增：

Archive。

记录：

所有Run。

例如：

```
Run1

远程流

Run2

近战流

Run3

召唤流

Run4

混合流
```

AI。

长期。

学习。

玩家。

玩家。

也学习。

AI。

---

# 15.12 Transparency UX（透明体验设计）

整个 AI 透明反馈按照不同时间点分层呈现，避免信息过载。

| 时机     | 展示内容               | 玩家目标         |
| ------ | ------------------ | ------------ |
| 楼层开始前  | Director Report    | 理解本层挑战       |
| 战斗中    | 白泽简短提示、镜界特效        | 感知 AI 正在介入   |
| 楼层结束   | Mirror Insight     | 理解 AI 如何评价自己 |
| Boss 前 | Boss Analysis      | 为最终试炼做准备     |
| Run 结束 | Timeline + Archive | 完成复盘与学习      |

这种分层反馈保证玩家在任何阶段都能获得恰当的信息，而不会因为一次性展示过多内容影响游戏节奏。

---

# 15.13 本章总结

AI Transparency Framework 是《山海镜》的**可解释层（Explainability Layer）**。

它将原本隐藏在后台的 AI Director 思考过程，通过：

* **Director Report**
* **Mirror Insight**
* **WhiteZe Commentary**
* **Timeline**
* **Archive**
* **Learning Report**

完整呈现给玩家。

最终形成：

> **Observe → Understand → Adapt → Grow**

而不是：

> **Observe → Confuse → Rage Quit**

透明性不仅提升了玩家信任，也让 AI Director 从一个看不见的算法，真正变成了《山海镜》世界中有思想、有性格、有记忆的"镜界观察者"。

---

# 第十六章 Technical Architecture（技术架构）

---

# 16.1 Technical Vision（技术目标）

《山海镜》的技术架构遵循三个核心目标：

> **AI 可扩展（Scalable）**
> **数据驱动（Data Driven）**
> **模块解耦（Modular）**

整个项目不依赖"写死逻辑"。

所有玩法均通过：

* 配置
* 标签
* 数据表
* AI Decision

驱动。

这样可以保证：

* 后期快速扩展
* 策划可独立调数值
* AI 可以自由组合规则

---

# 16.2 Overall Architecture（整体架构）

整个项目采用五层架构。

```text
┌─────────────────────────────────────────────┐
│                 UI Layer                    │
│  HUD / Director Report / WhiteZe / Menu     │
└─────────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────┐
│              Gameplay Layer                 │
│ Combat / Build / Boss / Encounter           │
└─────────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────┐
│             AI Director Layer               │
│ Mirror Mind / Director / Rule System        │
└─────────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────┐
│            Gameplay Framework               │
│ GAS / EventBus / SaveGame / DataTable       │
└─────────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────┐
│                Data Layer                   │
│ DT_Weapon / DT_Enemy / DT_Rule / JSON       │
└─────────────────────────────────────────────┘
```

---

# 16.3 Core Modules（核心模块）

整个游戏划分为九个核心模块。

| 模块                | 职责     |
| ----------------- | ------ |
| Combat System     | 战斗逻辑   |
| Build System      | 成长系统   |
| Encounter Manager | 遭遇管理   |
| Boss Manager      | Boss控制 |
| AI Director       | AI决策   |
| Mirror Mind       | 玩家认知   |
| Rule Manager      | 规则系统   |
| UI Manager        | 所有UI   |
| Save Manager      | 存档     |

模块之间：

只通过：

Interface。

通信。

---

# 16.4 Event Bus（事件总线）

整个游戏：

采用：

事件驱动。

而不是：

互相调用。

例如：

玩家：

攻击。

Combat：

不会：

通知：

AI。

而是：

广播。

```text
Attack Event
```

Event Bus：

收到。

随后：

```text
Combat

↓

Event Bus

↓

Recorder

↓

Mirror Mind
```

所有模块：

监听。

自己。

关心。

事件。

优势：

模块。

完全解耦。

---

# 16.5 Gameplay Ability System（GAS）

建议整个项目采用 UE5 Gameplay Ability System（GAS）。

统一管理：

* 技能
* Buff
* Debuff
* Rule Modifier
* Boss Skill
* Passive

例如：

```text
Ability

↓

Gameplay Effect

↓

Attribute

↓

Tag
```

所有技能共享统一生命周期。

避免重复开发。

---

# 16.6 Gameplay Tags（标签系统）

整个项目：

禁止：

使用：

字符串判断。

全部：

GameplayTag。

例如：

```text
Character.Melee

Character.Range

Enemy.Tank

Enemy.Rush

Rule.Ammo

Rule.Heal

Rule.Cooldown

Boss.Fire

Boss.Mirror
```

AI Director：

全部：

读取：

Tag。

程序：

不判断：

名字。

---

# 16.7 Data Driven（数据驱动）

整个游戏：

所有内容。

均来自：

DataTable。

例如：

Weapon。

来自：

```text
DT_Weapon
```

Enemy：

来自：

```text
DT_Enemy
```

Rule：

来自：

```text
DT_Rule
```

Passive：

来自：

```text
DT_Passive
```

Boss：

来自：

```text
DT_Boss
```

程序：

不写：

数值。

---

# 16.8 SaveGame Architecture（存档）

存档：

分三层。

---

Runtime

当前。

Run。

例如：

HP。

武器。

Rule。

---

Player

长期。

成长。

例如：

角色。

皮肤。

解锁。

---

Mirror

AI。

长期。

学习。

例如：

Run。

历史。

Player Archetype。

Director History。

---

# 16.9 AI Service（AI服务）

LLM：

不是：

游戏主逻辑。

而是：

服务。

流程：

```text
Floor End

↓

Generate Prompt

↓

Request LLM

↓

Receive JSON

↓

Validate

↓

Director Apply
```

如果：

失败。

进入：

Fallback。

---

# 16.10 JSON Validator（验证）

LLM。

任何输出。

必须：

验证。

例如：

```json
{
"Enemy":"Rush",
"Rule":"Ammo"
}
```

验证：

是否合法。

否则：

拒绝。

重新：

生成。

保证：

不会：

崩溃。

---

# 16.11 Fail Safe（容错）

AI。

失败。

怎么办？

三级Fallback。

Level1：

缓存。

上一层。

结果。

Level2：

本地Rule。

Table。

Level3：

固定Director。

流程。

玩家：

感觉不到：

AI失败。

---

# 16.12 Data Flow（数据流）

整局游戏数据流如下：

```text
Player Input
      │
      ▼
Combat System
      │
      ▼
Event Bus
      │
      ▼
Recorder
      │
      ▼
Mirror Mind
      │
      ▼
AI Director
      │
      ▼
Rule Manager
      │
      ▼
Encounter Generator
      │
      ▼
Next Floor
```

形成完整闭环。

---

# 16.13 Performance Budget（性能预算）

为了保证稳定运行，对各系统设置预算：

| 系统               |             目标 |
| ---------------- | -------------: |
| 战斗逻辑             | ≤ 1.5 ms/frame |
| AI Director（运行时） | 不占用实时帧，仅楼层结算执行 |
| Director 决策      |      ≤ 2 秒（异步） |
| UI 更新            | ≤ 0.5 ms/frame |
| SaveGame         |         后台异步写入 |
| Event Bus        |     常驻内存，低开销广播 |

AI 请求期间，玩家通过结算界面、强化选择等内容自然过渡，不阻塞游戏流程。

---

# 16.14 Project Folder Structure（工程目录规范）

建议 UE5 项目统一目录：

```text
Content/
├── Characters/
├── Enemies/
├── Boss/
├── Abilities/
├── Effects/
├── UI/
├── Maps/
├── AI/
├── DataTables/
├── Audio/
├── Materials/
├── VFX/
├── Blueprints/
└── Developer/
```

所有 DataTable、配置、Tag、Rule 统一放置，避免资源分散。

---

# 16.15 模块职责矩阵（Responsibility Matrix）

| 模块                | 是否负责 AI | 是否负责数值 | 是否负责表现 |
| ----------------- | :-----: | :----: | :----: |
| Combat System     |    ✗    |    ✓   |    ✓   |
| Build System      |    ✗    |    ✓   |    ✗   |
| Mirror Mind       |    ✓    |    ✗   |    ✗   |
| AI Director       |    ✓    |    ✗   |    ✗   |
| Rule Manager      |    ✓    |    ✓   |    ✗   |
| Encounter Manager |    ✓    |    ✓   |    ✗   |
| UI Manager        |    ✗    |    ✗   |    ✓   |
| Save Manager      |    ✗    |    ✗   |    ✗   |

这保证了职责单一，方便多人协作开发。

---

# 16.16 本章总结

Technical Architecture 为《山海镜》建立了一套**数据驱动、模块解耦、AI 可扩展**的底层框架。

核心原则包括：

* **事件驱动（Event Bus）**：降低模块耦合；
* **Gameplay Ability System（GAS）**：统一技能、Buff、Rule；
* **Gameplay Tags**：所有 AI 决策基于标签而非硬编码；
* **DataTable 配置化**：策划可独立扩展内容；
* **LLM 服务化**：AI 作为可替换服务，不影响核心逻辑；
* **多级容错（Fail Safe）**：保证离线、异常情况下游戏依然可运行。

这套架构不仅适用于比赛 Demo，也能够支持后续持续扩展，符合商业项目从原型到正式开发的技术演进路线。

---

# 第十七章 Game Economy & Balance Framework（数值与平衡体系）

---

# 17.1 Design Vision（设计目标）

《山海镜》的数值设计目标不是：

**让玩家越来越强。**

而是：

**让策略越来越丰富。**

因此。

整个数值系统遵循：

> **Power Growth < Strategy Growth**

即：

策略成长。

始终大于。

数值成长。

这是整个AI Native的基础。

---

# 17.2 Economy Architecture（经济架构）

整个游戏拥有四套独立经济。

```text
Combat Economy
        │
        ▼
Build Economy
        │
        ▼
Challenge Economy
        │
        ▼
AI Economy
```

四者共同组成整个游戏。

---

## 第一层

Combat Economy

负责：

即时战斗。

例如：

* HP
* Energy
* CD
* Dodge
* Damage

属于：

秒级经济。

---

## 第二层

Build Economy

负责：

成长。

例如：

武器。

Passive。

Skill。

Boss Reward。

属于：

分钟级经济。

---

## 第三层

Challenge Economy

负责：

Rule。

Enemy。

Arena。

属于：

Run级经济。

---

## 第四层

AI Economy

负责：

AI到底修改多少。

不是：

无限修改。

而是：

拥有：

Budget。

---

# 17.3 Difficulty Budget（难度预算）

AI。

不是：

无限增加。

难度。

而是：

每层拥有：

Difficulty Budget。

例如：

| Floor | Budget |
| ----- | ------ |
| 1     | 20     |
| 2     | 35     |
| 3     | 55     |
| 4     | 75     |
| 5     | 100    |

AI。

只能：

消费。

Budget。

例如：

```text
Rush

20

+

Heal Down

15

+

Elite

20

=

55
```

预算。

结束。

---

# 17.4 Reward Budget（奖励预算）

玩家。

获得。

强化。

同样：

拥有：

Reward Budget。

例如：

Floor3：

Reward：

40。

可以：

```text
Rare Passive

25

+

Gold

15

=

40
```

而不是：

无限奖励。

这样：

成长。

可控。

---

# 17.5 Rule Budget（规则预算）

Rule。

同样。

拥有：

Cost。

例如：

| Rule        | Cost |
| ----------- | ---- |
| Ammo Down   | 20   |
| Heal Down   | 20   |
| Cooldown    | 15   |
| Elite Buff  | 35   |
| Mirror Rule | 50   |

AI：

预算：

100。

只能：

选择：

部分。

---

# 17.6 Enemy Budget（敌人预算）

Enemy。

拥有：

Threat。

例如：

| Enemy   | Threat |
| ------- | ------ |
| Grunt   | 5      |
| Tank    | 20     |
| Rush    | 15     |
| Shooter | 18     |
| Elite   | 60     |

Generator：

根据：

Threat。

自动。

生成。

---

# 17.7 Build Budget（Build预算）

所有Build。

统一。

Power。

例如：

暴击流：

Power：

100。

持续流：

Power：

100。

召唤流：

Power：

98。

近战流：

Power：

102。

允许：

±5%。

禁止：

出现：

Power：

150。

整个生态。

保持：

稳定。

---

# 17.8 Rule Weight（规则权重）

Rule。

不是：

平均。

出现。

例如：

```text
Ammo

Weight

30
```

```text
Heal

Weight

25
```

```text
Mirror

Weight

5
```

Director：

根据：

Weight。

随机。

再结合。

Player Model。

修正。

---

# 17.9 Enemy Weight（敌人权重）

例如：

Floor3。

```text
Tank

20%

Rush

25%

Shooter

35%

Grunt

20%
```

玩家：

远程。

↓

Tank。

提升：

40%。

---

# 17.10 Passive Weight（强化权重）

整个强化。

采用：

动态权重。

例如：

玩家：

一直暴击。

↓

暴击Passive。

下降。

↓

近战Passive。

提升。

避免：

Build。

越来越固定。

---

# 17.11 Adaptive Reward（自适应奖励）

AI。

不仅：

修改：

敌人。

还修改：

奖励。

例如：

玩家：

一直远程。

AI：

可能。

提供：

```text
传奇近战强化
```

不是：

强迫。

而是：

诱导。

整个：

AI。

变成：

老师。

不是：

惩罚者。

---

# 17.12 Build Entropy（Build熵）

这是《山海镜》的原创数值指标。

传统 Roguelike 只统计：

DPS。

《山海镜》增加：

**Build Entropy（Build 熵）**。

用于衡量：

玩家 Build 的多样性。

例如：

```text
武器

一种

技能

一种

Passive

固定

↓

Entropy

0.21
```

如果：

```text
武器

三种

技能

四种

Passive

变化

↓

Entropy

0.81
```

AI：

鼓励：

Entropy。

高。

---

# 17.13 Director Fairness（公平系数）

AI。

拥有：

Fairness。

例如：

```text
Aggressive

80
```

```text
Fairness

95
```

Director。

不能：

超过：

Fairness。

否则：

拒绝。

生成。

例如：

连续：

三层。

Ammo Down。

↓

非法。

---

# 17.14 Dynamic Balance（动态平衡）

整个游戏。

不是：

Designer。

平衡。

而是：

AI。

平衡。

例如：

过去：

100局。

统计：

远程。

胜率：

68%。

AI。

自动。

提高。

Tank。

Weight。

形成：

Live Balance。

---

# 17.15 Mirror Balance（镜界平衡）

Mirror Mind 不只学习玩家，还学习整个游戏生态。

系统会持续统计：

| 指标            | 用途               |
| ------------- | ---------------- |
| Build 胜率      | 判断是否存在明显优势流派     |
| Rule 适应率      | 判断规则是否真正促进策略变化   |
| Enemy 威胁值     | 校正 Threat Budget |
| Boss 通关率      | 调整阶段节奏           |
| Build Entropy | 衡量策略多样性          |

Mirror Mind 不会自动修改数值，而是生成平衡建议，供策划验证后更新配置表。

---

# 17.16 数值验证指标（Balance KPIs）

为了确保系统持续健康，建立以下核心指标：

| KPI                | 目标值     |
| ------------------ | ------- |
| 单一 Build 使用率       | ≤40%    |
| Rule 平均适应率         | 50%~70% |
| Build Entropy      | ≥0.60   |
| Boss 首次通关率         | 30%~45% |
| AI 调整被玩家感知率（测试）    | ≥80%    |
| 玩家认为 AI "作弊"比例（测试） | ≤10%    |

这些 KPI 将作为版本迭代的重要依据。

---

# 17.17 本章总结

Game Economy & Balance Framework 为《山海镜》建立了完整的数值与平衡体系。

它不是围绕"角色变强"设计，而是围绕：

* **Budget（预算）**
* **Weight（权重）**
* **Entropy（策略多样性）**
* **Fairness（公平性）**

四个核心概念构建整个游戏生态。

最终实现：

> **AI 不负责制造困难，而负责维持策略生态的长期健康。**

---

# 第十八章 AI Evaluation Framework（AI评估体系）

---

# 18.1 System Vision（系统定位）

AI Evaluation Framework（AEF）用于持续评估 AI Director 的设计质量、玩家适应情况以及整体游戏生态健康度。

在传统游戏中，难度通常通过设计师主观调整完成。而《山海镜》中，AI Director 会持续参与挑战设计，因此必须建立一套客观、可量化的评估体系。

AI Evaluation Framework 的目标不是评价玩家，而是评价 **AI 自己**。

它回答四个核心问题：

1. AI 是否真正理解了玩家？
2. AI 是否成功促使玩家改变策略？
3. AI 是否保持了公平性？
4. AI 是否让游戏变得更有趣，而不是更困难？

整个系统既服务于开发阶段的调试，也服务于正式版本的数据分析与持续平衡。

---

# 18.2 Evaluation Architecture（评估架构）

整个评估系统采用五层架构。

```text
Gameplay
      │
      ▼
Telemetry Collection
      │
      ▼
Mirror Analytics
      │
      ▼
Evaluation Engine
      │
      ▼
Dashboard / Balance Report
```

五层职责如下：

| 层级                | 职责         |
| ----------------- | ---------- |
| Gameplay          | 玩家真实游戏行为   |
| Telemetry         | 行为数据采集     |
| Mirror Analytics  | 数据分析与画像更新  |
| Evaluation Engine | AI效果评估     |
| Dashboard         | 可视化展示与平衡建议 |

---

# 18.3 AI Success Definition（AI成功定义）

《山海镜》中，AI Director 的成功并不意味着玩家死亡率提高。

AI 成功必须同时满足以下四项标准：

| 指标                  | 描述           |
| ------------------- | ------------ |
| Strategy Adaptation | 玩家主动改变策略     |
| Build Diversity     | 玩家尝试更多 Build |
| Fair Challenge      | 玩家认为挑战公平     |
| High Engagement     | 玩家保持探索兴趣     |

因此：

> AI 成功 ≠ 玩家失败

真正成功的 AI，是让玩家不断思考新的解决方案。

---

# 18.4 Key Performance Indicators（核心KPI）

系统定义八项核心指标。

---

## KPI 1：Build Diversity Index（BDI）

用于衡量玩家是否持续尝试不同 Build。

计算参考：

* 武器切换率
* 技能变化率
* Passive 更换率
* Archetype 变化率

最终输出：

```
0~100
```

评分标准：

| 分数     | 含义       |
| ------ | -------- |
| 0~20   | 单一 Build |
| 20~50  | 少量变化     |
| 50~80  | 多样化      |
| 80~100 | 高度适应     |

目标：

平均 ≥60。

---

## KPI 2：Strategy Adaptation Rate（SAR）

衡量 AI 是否真正促使玩家改变策略。

例如：

```
AI：

增加盾兵

↓

玩家：

改近战

↓

Adaptation
```

计算方式：

```
适应次数

──────────────

AI挑战次数
```

目标：

50%~70%。

如果达到100%。

说明 AI 太容易。

如果只有10%。

说明 AI 设计失败。

---

## KPI 3：Rule Acceptance Rate（RAR）

衡量玩家是否愿意接受 Rule Modifier。

例如：

AI：

Ammo Down

↓

玩家：

继续游戏

↓

Accept

如果：

直接退出。

↓

Reject。

目标：

RAR ≥85%。

---

## KPI 4：Director Prediction Accuracy（DPA）

Mirror Mind 会预测玩家 Build。

例如：

AI预测：

```
Ranged

Confidence

92%
```

下一层：

玩家：

是否：

继续：

远程？

如果：

正确。

Prediction：

Success。

计算：

```
预测正确次数

──────────────

预测总次数
```

目标：

75%。

---

## KPI 5：Fairness Index（FI）

玩家是否认为：

AI 公平。

计算来源：

* 连续失败次数
* 相同 Rule 重复率
* Rule Conflict
* Challenge Budget

最终：

```
0~100
```

目标：

> 80。

---

## KPI 6：Player Frustration Index（PFI）

这是商业项目常见指标。

统计：

玩家是否：

开始：

烦躁。

例如：

连续：

死亡。

↓

连续：

重复Boss。

↓

退出。

↓

快速Restart。

AI：

开始：

降低：

Pressure。

---

## KPI 7：Mirror Intelligence Score（MIS）

这是整个项目原创指标。

用于评价：

Mirror Mind。

是否：

真正：

理解玩家。

组成：

```
Prediction

+

Adaptation

+

Fairness

+

Explainability
```

例如：

| 模块             | Score |
| -------------- | ----: |
| Prediction     |    86 |
| Adaptation     |    74 |
| Fairness       |    93 |
| Explainability |    88 |

最终：

MIS：

85。

---

## KPI 8：AI Explainability Score（AES）

衡量玩家是否理解 AI。

统计：

例如：

玩家：

是否：

查看：

Director Report。

↓

是否：

打开：

Mirror Timeline。

↓

是否：

查看：

Archive。

说明：

玩家：

真正。

理解。

AI。

目标：

> 70%。

---

# 18.5 Director Evaluation Pipeline（导演评估流程）

每局游戏结束后，系统自动完成 Director 评估。

```text
Gameplay Finished
        │
        ▼
Collect Telemetry
        │
        ▼
Generate Player Report
        │
        ▼
Evaluate Director
        │
        ▼
Generate Balance Report
        │
        ▼
Update AI Statistics
```

开发版会生成详细报告，正式版仅保存统计数据。

---

# 18.6 Telemetry Collection（数据采集）

所有关键行为统一记录。

| 类别      | 数据                   |
| ------- | -------------------- |
| Combat  | 命中、受击、闪避、死亡          |
| Build   | 武器、技能、Passive        |
| AI      | Rule、Enemy、Challenge |
| UX      | 查看报告、停留时间            |
| Session | 游戏时长、退出时间            |

所有数据采用统一事件格式，便于后续分析。

---

# 18.7 AI Dashboard（AI仪表盘）

开发版本提供实时 Dashboard。

主要模块：

### Player Model

显示：

* Archetype
* Build
* Adaptation
* Confidence

---

### Director Decision

显示：

* 当前 Rule
* Challenge Budget
* Enemy Weight
* Resource Weight

---

### Evaluation

显示：

* SAR
* BDI
* DPA
* FI
* MIS

方便策划快速定位问题。

---

# 18.8 Offline Replay Analyzer（离线回放分析）

每局游戏结束后生成 Replay Data。

开发者可以回放：

* 玩家行为
* AI 决策
* Rule 修改
* Enemy 生成
* Boss 技能选择

并逐帧查看：

> 为什么 AI 会在这一层增加盾兵？

Replay 同时记录 Mirror Mind 的推理结果，便于 Debug。

---

# 18.9 AI Failure Detection（AI失效检测）

系统自动检测以下异常：

| 异常           | 处理                  |
| ------------ | ------------------- |
| AI 连续预测错误    | 降低 Confidence 权重    |
| Rule 重复率过高   | 降低 Rule Weight      |
| 玩家连续放弃 Build | 检查奖励系统              |
| 玩家大量退出       | 降低 Challenge Budget |

保证 AI 能持续修正自己的策略。

---

# 18.10 Live Balance Workflow（动态平衡流程）

正式版采用：

```
Telemetry

↓

Analytics

↓

Balance Suggestion

↓

Designer Review

↓

Config Update
```

注意：

Mirror Mind **不会直接修改正式版本数值**。

AI 只负责：

提出建议。

最终：

策划：

确认。

保证：

商业项目：

安全。

---

# 18.11 A/B Test Framework（A/B测试）

为了验证 AI Director 的真实价值，可设计两组实验：

| 组别 | 内容                 |
| -- | ------------------ |
| A组 | 固定关卡，无 AI Director |
| B组 | AI Director 全功能开启  |

比较：

* 通关率
* Build Diversity
* 平均游戏时长
* 玩家满意度
* AI 感知率

如果 B 组在策略多样性和重复游玩意愿上明显优于 A 组，则说明 AI Native 设计达到了预期。

---

# 18.12 Evaluation Summary（本章总结）

AI Evaluation Framework 是《山海镜》的**验证层（Validation Layer）**。

它不是为了评价玩家，而是持续验证 AI Director 是否真正完成了自己的设计目标：

* 是否理解玩家；
* 是否引导玩家改变策略；
* 是否保持公平与可解释；
* 是否提升了长期重复游玩体验。

通过 **KPI、Telemetry、Replay、Dashboard、A/B Test** 五大体系，《山海镜》的 AI Director 不再只是一个"会调难度"的系统，而是一个**可量化、可验证、可持续优化的 AI Native 游戏框架**。

---

## 专业建议（这一章还能继续提升）

如果这是我要提交腾讯游戏开发大赛的最终版，我还会在这一章最后增加两个原创指标：

1. **Mirror Cognitive Score（MCS）**：衡量 AI 对玩家长期行为理解的准确性，而不仅是单局表现。
2. **Strategy Evolution Score（SES）**：衡量玩家是否真正形成了新的策略，而不是被迫临时更换 Build。

这两个指标可以直接作为答辩中的核心创新点，也能进一步强化《山海镜》作为 AI Native 游戏的研究价值。
很好。

下面开始写**第十九章 UX Design Framework（用户体验设计框架）**。

这一章我不会写成"UI怎么摆"。

真正商业游戏（《Hades》《Returnal》《Dead Space》《战神》《LOL》）讨论的是：

> **UX（User Experience）**

UI只是其中一部分。

对于《山海镜》来说，UX 的目标只有一句话：

> **让玩家始终知道 AI 在思考什么，而不会觉得 AI 在作弊。**

---

# 第十九章 UX Design Framework（用户体验设计框架）

---

# 19.1 UX Vision（设计目标）

《山海镜》的用户体验围绕 **Mirror Director（镜界导演）** 展开。

玩家在整个游戏过程中需要始终保持三个认知：

1. AI 正在观察我。
2. AI 能理解我的玩法。
3. AI 的所有调整都是有原因的。

因此，UX 的职责不是展示信息，而是建立玩家与 AI Director 的信任关系。

整个 UX 的设计目标为：

> **Visible Intelligence（可见的智能）**

AI 必须被玩家"看见"。

---

# 19.2 UX Design Philosophy（设计原则）

整个 UX 遵循五项原则。

---

## Principle 01

### Information Before Challenge

所有重要信息必须发生在挑战之前。

例如：

进入楼层前。

先看到：

Director Report。

而不是：

进入战斗后才知道。

---

## Principle 02

### AI Is A Character

白泽。

不是：

系统UI。

而是：

角色。

因此：

所有AI信息。

均由：

白泽。

表达。

避免：

系统弹窗。

---

## Principle 03

### Layered Information

信息：

分层。

不是：

一次：

全部展示。

例如：

战斗。

只显示：

必要信息。

结算。

再显示：

完整分析。

降低：

认知负荷。

---

## Principle 04

### Learn By Playing

新玩家。

不用：

阅读教程。

而是：

通过：

Director Report。

逐渐理解：

AI。

---

## Principle 05

### Minimal Disruption

任何UI。

不得：

打断。

战斗。

AI介入。

必须：

自然。

---

# 19.3 UX Information Hierarchy（信息层级）

整个游戏信息分五层。

```text
Moment Information

↓

Combat Information

↓

Floor Information

↓

Run Information

↓

Meta Information
```

---

## 第一层

Moment

持续：

1秒。

例如：

伤害数字。

暴击。

闪避。

Rule触发。

---

## 第二层

Combat

整个战斗。

例如：

HP。

Skill。

Rule。

Boss。

---

## 第三层

Floor

每层。

例如：

Director Report。

Mirror Insight。

---

## 第四层

Run

整局。

例如：

Mirror Timeline。

Build History。

---

## 第五层

Meta

长期。

例如：

Archive。

Mirror Memory。

---

# 19.4 HUD Framework（HUD框架）

HUD 采用"轻量信息 + AI 状态"双层布局。

```
┌────────────────────────────────────────────┐
│ HP      Energy      Rule Icon      MiniMap │
│                                            │
│                                            │
│              Gameplay Area                 │
│                                            │
│                                            │
│ Skill1 Skill2 Dodge Weapon      AI Status  │
└────────────────────────────────────────────┘
```

HUD 永远保持：

简洁。

AI 信息。

独立。

显示。

---

# 19.5 Director Report（导演报告）

这是：

整个游戏。

最重要UI。

每层。

开始。

展示。

---

## Layout

```
━━━━━━━━━━━━━━━━━━━━━━━━━━

白泽观察报告

━━━━━━━━━━━━━━━━━━━━━━━━━━

玩家画像

Build分析

AI判断

本层调整

建议策略

白泽评价

━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## 内容

玩家：

一分钟。

即可：

理解。

AI。

为什么：

修改。

---

# 19.6 Mirror Insight（镜界洞察）

这是：

AI。

真正：

看到的。

例如：

```
Build集中度

★★★★☆

适应能力

★★☆☆☆

资源管理

★★★★★

AI置信度

87%
```

这里。

不展示：

公式。

只展示：

结论。

---

# 19.7 Rule Card（规则卡）

进入楼层时。

Rule。

采用：

卡牌。

形式。

例如：

```
镜界规则

【弹药不足】

弹药掉率

-25%

持续：

本层

建议：

尝试近战。
```

Rule。

永远：

提前。

知道。

---

# 19.8 WhiteZe Dialogue（白泽对白）

对白。

采用：

参数化。

生成。

例如：

Build。

固定。

↓

对白：

> 你的战斗开始变得可以预测。

Build。

变化。

↓

对白：

> 很好，你终于开始寻找新的道路。

玩家：

感觉。

白泽。

一直。

观察。

自己。

---

# 19.9 AI Notification（AI提示）

战斗中。

AI。

只允许：

轻提示。

例如：

Boss。

获得：

Mirror Skill。

↓

白泽：

> 看来，它已经找到你的弱点。

整个提示：

≤2秒。

不会：

遮挡。

画面。

---

# 19.10 Boss Analysis（Boss分析）

Boss。

开始前。

展示：

分析。

例如：

```
烛龙

正在分析你……

识别：

远程Build

适应能力：

A

预测：

高机动

建议：

注意冲锋技能。
```

Boss。

开始。

玩家：

已有：

心理准备。

---

# 19.11 Mirror Timeline（镜界时间轴）

Run。

结束。

自动。

生成。

```
Floor1

建立画像

↓

Floor2

增加Rush

↓

Floor3

玩家换Build

↓

Floor4

Rule改变

↓

Floor5

Boss选择Mirror Dash
```

整个Run。

形成：

故事。

---

# 19.12 Mirror Archive（镜界档案）

长期。

记录。

例如：

```
Run 12

远程流

Run 13

近战流

Run 14

混合流

Run 15

法术流
```

Archive。

帮助：

玩家。

理解：

自己。

---

# 19.13 Accessibility（无障碍设计）

为保证更多玩家能够体验 AI Native 玩法，系统提供以下辅助选项：

| 功能     | 说明                     |
| ------ | ---------------------- |
| 色盲模式   | Rule 图标增加形状区分          |
| 字体缩放   | Director Report、对白支持缩放 |
| 战斗字幕   | 白泽语音同步字幕               |
| 高对比度   | Rule、Boss 提示增强可读性      |
| 简化 HUD | 隐藏非必要信息                |

所有 AI 信息都必须支持文字形式，不依赖颜色或音效单独传达。

---

# 19.14 New Player Onboarding（新手引导）

新手教程采用"渐进式学习"。

第一层：

AI：

不修改。

只观察。

第二层：

第一次：

Director Report。

第三层：

第一次：

Rule。

第四层：

第一次：

Boss分析。

第五层：

完整。

AI。

系统。

玩家：

不会：

一次：

学习。

全部。

机制。

---

# 19.15 UX Flow（完整体验流程）

```text
进入楼层

↓

Director Report

↓

Rule Card

↓

开始战斗

↓

WhiteZe提示

↓

Boss Analysis

↓

Mirror Timeline

↓

Archive
```

整个UX。

围绕：

Mirror。

展开。

---

# 19.16 Cognitive Load Control（认知负荷控制）

AI Native 游戏天然容易造成信息过载，因此 UX 必须控制玩家每个阶段的信息量。

| 场景     | 最大信息量 |
| ------ | ----- |
| 战斗     | 3 条以内 |
| 楼层开始   | 5 条以内 |
| Boss 前 | 4 条以内 |
| Run 结算 | 完整展示  |

原则：

**战斗时少、结算时多；即时反馈短、复盘分析长。**

---

# 19.17 UX Validation（体验验证）

UX 是否成功，采用以下指标验证：

| 指标                      | 目标   |
| ----------------------- | ---- |
| 玩家阅读 Director Report 比例 | ≥80% |
| 玩家理解 Rule 含义比例（测试）      | ≥90% |
| 玩家认为 AI 公平比例            | ≥85% |
| 玩家认为 AI "作弊"比例          | ≤10% |
| 新玩家完成教程比例               | ≥95% |

---

# 19.18 本章总结

UX Design Framework 是《山海镜》的**玩家沟通层（Player Communication Layer）**。

它通过：

* **Director Report**
* **Mirror Insight**
* **Rule Card**
* **Boss Analysis**
* **Mirror Timeline**
* **Archive**

建立起玩家与 AI Director 的长期信任关系。

对于《山海镜》而言，优秀的 UX 并不是让界面更漂亮，而是让玩家始终理解：

> **AI 不是在针对我，而是在帮助我突破自己的策略边界。**

整个 UX Framework 最终服务于一个目标：

> **让 AI 的每一次决策都可见、可理解、可学习、可接受。**

这也是 AI Native 游戏区别于传统动态难度系统的重要体验基础。

---

# 第二十章 Art Direction Bible（美术圣经）

---

# 20.1 Art Vision（视觉愿景）

《山海镜》的视觉设计围绕一个核心概念展开：

> **神话，并非过去，而是正在被 AI 重构。**

因此。

整个美术并不是传统国风。

也不是纯幻想风格。

而是：

> **New Chinese Myth × Mirror Reality × AI Native**

整个世界应当具有：

* 东方神话的厚重感
* 镜面空间的不真实感
* AI重构世界的动态变化感

因此。

玩家看到的不是：

《山海经》。

而是：

**AI眼中的《山海经》。**

---

# 20.2 Visual Design Philosophy（视觉设计原则）

整个项目遵循五项视觉原则。

---

## Principle 01

Reality Before Fantasy

所有设计。

先符合：

真实结构。

再加入：

神话元素。

例如：

穷奇。

首先是一只：

真实猛兽。

然后。

加入：

神话特征。

而不是：

直接：

堆花纹。

---

## Principle 02

Readable First

玩家。

必须：

0.3秒。

认出：

敌人。

因此：

轮廓。

永远。

优先。

细节。

其次。

---

## Principle 03

Mirror Language

所有：

AI。

介入。

必须：

拥有：

统一视觉语言。

例如：

镜面。

裂纹。

碎片。

折射。

蓝白色流光。

玩家。

看到。

立即知道：

AI。

正在：

修改世界。

---

## Principle 04

Low Saturation

整体：

低饱和。

局部：

高亮。

例如：

世界。

灰绿。

Boss技能。

金色。

Rule。

青蓝。

保证：

重点突出。

---

## Principle 05

Gameplay First

任何美术。

不得：

影响：

玩法识别。

例如：

技能范围。

必须：

准确。

优先于：

炫酷。

---

# 20.3 World Art Style（世界风格）

整个镜界采用：

三层世界。

---

## 第一层

Human World

玩家进入。

色彩：

正常。

真实。

暖色。

---

## 第二层

Mirror World

主要玩法。

色彩：

冷灰。

青蓝。

镜面。

这是：

主要世界。

---

## 第三层

Ancient Myth

Boss。

剧情。

神迹。

大量：

金色。

黑曜石。

赤红。

形成：

压迫感。

---

# 20.4 Color Script（色彩体系）

整个游戏建立统一色彩脚本。

| 系统    | 主色 | 辅色 | 设计目的  |
| ----- | -- | -- | ----- |
| 玩家    | 青白 | 金色 | 希望、成长 |
| 白泽    | 银白 | 青蓝 | 理性、观察 |
| 镜界    | 冷灰 | 镜蓝 | AI介入  |
| 普通敌人  | 深灰 | 墨绿 | 山海异兽  |
| Elite | 深红 | 金橙 | 高威胁   |
| Boss  | 黑金 | 赤红 | 神性压迫  |
| Rule  | 冰蓝 | 白光 | 系统规则  |

所有UI。

统一。

使用：

Mirror Blue。

---

# 20.5 Shape Language（造型语言）

所有角色遵循统一轮廓设计。

---

## Player

关键词：

稳定。

成长。

希望。

轮廓：

直线。

对称。

---

## WhiteZe

关键词：

智慧。

观察。

平衡。

轮廓：

圆形。

柔和。

---

## Enemy

关键词：

危险。

未知。

攻击。

轮廓：

三角。

尖锐。

---

## Boss

关键词：

神。

巨大。

不可预测。

轮廓：

大量。

曲线。

巨大比例。

---

# 20.6 Environment Bible（场景规范）

场景采用模块化设计。

基础模块：

* 地面
* 墙体
* 神树
* 镜面
* 遗迹
* 山海图腾

所有地图通过不同组合形成新关卡，保证重复游玩时既保持新鲜感，又控制开发成本。

---

# 20.7 Character Bible（角色规范）

角色设计遵循：

> **Silhouette First（轮廓优先）**

要求：

* 远距离即可识别职业。
* 动作幅度明显。
* 武器成为第一识别点。

例如：

力士：

巨大武器。

宽肩。

厚重。

方士：

轻装。

长袍。

符纸。

飘带。

---

# 20.8 Enemy Bible（敌人规范）

所有异兽统一设计规范。

| 类型         | 关键词 | 主轮廓 |
| ---------- | --- | --- |
| Grunt      | 野兽  | 小型  |
| Tank       | 巨石  | 方形  |
| Rush       | 猎豹  | 三角  |
| Shooter    | 长蛇  | 细长  |
| Controller | 植物  | 不规则 |
| Elite      | 神兽  | 巨大  |

玩家：

第一眼。

知道：

敌人。

职责。

---

# 20.9 WhiteZe Visual Identity（白泽视觉规范）

白泽是整个项目最重要的视觉 IP。

设计关键词：

* 神圣
* 理性
* 非攻击性
* AI化

特征：

* 白银色毛发
* 青蓝色瞳孔
* 半透明镜面纹路
* 身体局部出现几何光纹

当 AI Director 做出决策时，白泽周围会出现镜面裂纹和数据流粒子，而不是传统魔法效果。

---

# 20.10 VFX Bible（特效规范）

所有特效采用统一规则：

| 类型       | 表现   |
| -------- | ---- |
| 普通攻击     | 白色粒子 |
| 暴击       | 金色爆裂 |
| Rule 生效  | 镜面碎片 |
| AI 修改    | 蓝白流光 |
| Boss 技能  | 黑金符文 |
| Build 升级 | 金色镜纹 |

特效层级必须遵循：

玩家 > Boss > Elite > 普通敌人 > 环境。

避免视觉混乱。

---

# 20.11 Lighting Bible（灯光规范）

灯光不仅承担照明，也承担叙事。

| 场景         | 光照          |
| ---------- | ----------- |
| 普通探索       | 自然冷光        |
| Rule 修改    | 光线轻微闪烁      |
| WhiteZe 出现 | 青白顶光        |
| Boss 战     | 强方向光 + 阴影增强 |
| Mirror 世界  | 高反射低漫反射     |

AI Director 每次介入，都伴随灯光微调，使玩家形成条件反射。

---

# 20.12 UI Bible（界面规范）

UI 整体风格：

> **Mirror Minimalism（镜界极简）**

规范：

* 半透明玻璃面板
* 镜纹边框
* 青蓝发光描边
* 少阴影
* 少装饰

所有 AI 信息使用统一 UI 模板，避免风格割裂。

---

# 20.13 Animation Bible（动画规范）

动画遵循：

> Gameplay First。

例如：

| 动画         | 时长         |
| ---------- | ---------- |
| 普攻前摇       | 0.15~0.25s |
| 闪避         | 0.35~0.45s |
| Boss 重击    | ≥0.8s      |
| Rule 生效    | ≤0.5s      |
| WhiteZe 登场 | 2.0s       |

所有 Boss 大招必须具有清晰前摇和结束动作，保证可学习性。

---

# 20.14 Audio Direction（音频方向）

音频采用三层结构：

* **世界环境**：风、水、远古遗迹、神兽低鸣。
* **AI 层**：镜面碎裂、数据流、白泽出现的专属音色。
* **战斗层**：武器命中、技能释放、Boss 重击。

AI Director 的每一次介入，都应拥有统一的"镜界提示音"，让玩家无需查看 UI 即能意识到世界发生变化。

---

# 20.15 Mood Board（情绪板）

整个项目的视觉参考建议如下：

| 方向   | 参考作品      | 借鉴内容       |
| ---- | --------- | ---------- |
| 动作体验 | Hades     | 战斗可读性、技能反馈 |
| 东方神话 | 黑神话：悟空    | 山海异兽、材质表现  |
| 镜界氛围 | Control   | 超现实空间、几何秩序 |
| 光影   | Returnal  | 冷色科幻与压迫感   |
| UI   | Destiny 2 | 极简信息架构     |

注意：

参考的是：

设计语言。

不是：

美术风格。

---

# 20.16 Art Production Pipeline（美术制作流程）

```text
Concept Art
      │
      ▼
Style Review
      │
      ▼
3D Modeling
      │
      ▼
Rigging
      │
      ▼
Animation
      │
      ▼
VFX
      │
      ▼
Integration
      │
      ▼
Gameplay Review
      │
      ▼
Art Polish
```

任何资源进入游戏前，都必须通过 **Gameplay Review**，确认不会影响战斗可读性。

---

# 20.17 Art Quality Checklist（美术验收标准）

| 检查项     | 验收标准              |
| ------- | ----------------- |
| 轮廓识别    | 3 米外可辨认角色类型       |
| 技能可读性   | 攻击范围与特效一致         |
| 色彩统一    | 符合 Color Script   |
| AI 视觉反馈 | Rule 修改具有统一镜界语言   |
| Boss 表现 | 兼顾压迫感与可读性         |
| UI 一致性  | 全部采用 Mirror UI 规范 |

---

# 20.18 本章总结

Art Direction Bible 是《山海镜》的**视觉规范层（Visual Identity Layer）**。

它不仅定义了"画什么"，更定义了：

* 为什么这样画；
* 如何让 AI Director 被玩家感知；
* 如何保证玩法与美术统一；
* 如何让整个项目形成统一的品牌视觉。

最终，《山海镜》的视觉核心可以浓缩为一句话：

> **不是在描绘《山海经》，而是在描绘一个由 AI 不断重构的神话世界。**

这套 Art Bible 可以直接作为角色、美术、UI、VFX、动画及外包团队的统一制作规范，也是商业项目进入量产阶段的重要基础。
很好。

---

# 第二十一章 Production Plan（研发计划）

---

# 21.1 Production Vision（研发目标）

《山海镜》的研发目标并非完成一款功能完整的大型商业游戏，而是在有限时间内，验证 **AI Native Gameplay** 的核心设计理念。

项目遵循：

> **Vertical Slice First（先完成完整体验，再扩展内容）**

整个开发优先保证：

* AI Director 可运行
* 玩家完整体验一局
* 核心玩法闭环验证成功

而不是：

大量内容。

---

# 21.2 Production Philosophy（研发原则）

整个开发遵循五项原则。

---

## Principle 01

Core Before Content

先完成：

玩法。

再制作：

内容。

例如：

先完成：

AI Director。

再增加：

怪物。

---

## Principle 02

Playable Every Week

每周。

必须：

可玩。

而不是：

一直开发。

最后：

合并。

---

## Principle 03

Feature Freeze

MVP。

功能冻结。

新增：

需求。

进入：

Version1.1。

---

## Principle 04

Data Driven

所有：

参数。

进入：

DataTable。

程序。

不写。

数值。

---

## Principle 05

AI First

所有系统。

必须：

服务：

AI Director。

不能：

脱离。

AI。

单独设计。

---

# 21.3 Team Structure（团队结构）

MVP：

两人团队。

---

## Lead Programmer

职责：

* UE5 项目架构
* Gameplay Framework
* Combat
* AI Director
* Mirror Mind
* LLM 接口
* DataTable
* Rule System
* Build System
* Save
* Debug
* Performance

---

## Lead Artist

职责：

* Character
* Enemy
* Boss
* UI
* Icon
* VFX
* Animation
* Audio
* Trailer
* 宣传素材

---

# 21.4 Development Pipeline（开发流程）

采用：

四阶段。

```text id="dmu63t"
Prototype

↓

Vertical Slice

↓

Content

↓

Polish
```

---

## Prototype

目标：

验证：

AI。

可以：

修改：

关卡。

周期：

2周。

---

## Vertical Slice

完成：

完整。

一局。

可玩。

周期：

3周。

---

## Content

增加：

Boss。

Enemy。

Build。

Rule。

周期：

2周。

---

## Polish

优化：

Bug。

体验。

UI。

性能。

周期：

1周。

---

# 21.5 Milestone（里程碑）

| 阶段 | 时间    | 交付成果              |
| -- | ----- | ----------------- |
| M0 | Week1 | UE5工程、基础移动、攻击     |
| M1 | Week2 | Combat Prototype  |
| M2 | Week3 | AI Director Alpha |
| M3 | Week4 | Mirror Mind Beta  |
| M4 | Week5 | 完整5层流程            |
| M5 | Week6 | Boss战完成           |
| M6 | Week7 | UI、白泽、Rule完成      |
| M7 | Week8 | Demo Freeze       |

---

# 21.6 Sprint Plan（冲刺计划）

采用：

一周。

一个Sprint。

---

Sprint1

Combat。

Movement。

Input。

---

Sprint2

Enemy。

Encounter。

---

Sprint3

Player Model。

Director。

---

Sprint4

Rule。

Mirror。

---

Sprint5

Boss。

---

Sprint6

UI。

WhiteZe。

---

Sprint7

Art。

VFX。

---

Sprint8

QA。

Fix。

Submit。

---

# 21.7 Story Point（工作量评估）

采用：

敏捷开发。

Story Point。

估算。

| 模块            | SP | 优先级 |
| ------------- | -: | :-: |
| Combat        | 21 |  P0 |
| Build         | 13 |  P0 |
| AI Director   | 34 |  P0 |
| Mirror Mind   | 21 |  P0 |
| Rule Modifier | 13 |  P0 |
| Encounter     |  8 |  P1 |
| Boss          | 13 |  P1 |
| UI            |  8 |  P1 |
| Art           | 34 |  P0 |
| Audio         |  5 |  P2 |
| Save          |  5 |  P1 |
| QA            |  8 |  P0 |

总 Story Point：

约：

163。

---

# 21.8 Priority Matrix（优先级）

采用：

MoSCoW。

| 类型         | 内容                                  |
| ---------- | ----------------------------------- |
| Must       | Combat、AI Director、Mirror Mind、Boss |
| Should     | Rule、UI、Build                       |
| Could      | Voice、Archive、Replay                |
| Won't（MVP） | Shop、Meta Progression、Multiplayer   |

保证：

Demo。

完成。

---

# 21.9 Risk Assessment（风险评估）

| 风险        |  等级 | 应对方案                              |
| --------- | :-: | --------------------------------- |
| LLM 接口异常  |  高  | 本地 Rule Table 回退                  |
| AI 调整失衡   |  高  | Challenge Budget + Fairness Guard |
| UE5 性能不足  |  中  | 对象池、LOD、异步加载                      |
| 美术资源延期    |  中  | 使用占位资源，后期替换                       |
| Boss 开发延期 |  中  | MVP 保留单 Boss                      |
| API 成本过高  |  中  | 每层仅调用一次，缓存 Prompt                 |

---

# 21.10 Quality Assurance（QA）

建立五层测试。

---

Unit Test

程序。

---

Gameplay Test

玩法。

---

AI Test

Director。

---

Balance Test

Rule。

---

User Test

玩家。

---

所有：

版本。

必须：

通过。

五层。

测试。

---

# 21.11 Playtest Plan（玩家测试）

计划三轮。

---

Alpha

开发组。

内部。

---

Beta

同学。

体验。

---

RC

陌生玩家。

测试。

重点验证：

* AI 是否容易理解；
* Rule 是否公平；
* 玩家是否愿意改变 Build；
* WhiteZe 是否建立人格。

---

# 21.12 Submission Checklist（比赛交付）

最终提交：

| 内容          | 状态 |
| ----------- | -- |
| 可运行 Demo    | □  |
| GDD         | □  |
| PPT         | □  |
| Trailer     | □  |
| Gameplay 视频 | □  |
| AI 架构图      | □  |
| UE5 工程      | □  |
| Readme      | □  |

确保：

评委。

5分钟。

理解。

整个游戏。

---

# 21.13 Version Roadmap（版本规划）

```text id="ztb9eo"
Prototype
      │
      ▼
MVP（比赛版本）
      │
      ▼
Version 1.1
（温和模式、商店）
      │
      ▼
Version 1.5
（事件、更多Boss）
      │
      ▼
Version 2.0
（Mirror World）
      │
      ▼
Version 3.0
（Persistent AI）
```

未来版本重点：

* **V1.1**：完善玩法深度，增加内容丰富度。
* **V1.5**：开放更多 AI 事件与 Boss，扩展 Rule 库。
* **V2.0**：引入完整 Mirror World 世界机制。
* **V3.0**：实现跨 Run 的 Persistent AI（持续学习 AI），形成真正的长期 AI Native 游戏体验。

---

# 21.14 Postmortem Plan（项目复盘）

比赛结束后，团队需完成一次正式复盘。

内容包括：

* 哪些 AI 设计验证成功；
* 哪些 Rule 没有产生预期效果；
* 玩家最喜欢的 Build；
* 玩家最不理解的 AI 行为；
* 技术架构是否支持后续扩展；
* 美术与玩法是否保持统一。

复盘结果将直接形成下一版本的开发输入，而不是停留在经验总结。

---

# 21.15 Success Criteria（项目成功标准）

项目成功不仅以比赛成绩衡量，更以 AI Native 设计目标是否实现作为标准。

| 目标       | 验收标准            |
| -------- | --------------- |
| AI 可感知   | 玩家明确知道 AI 在观察自己 |
| AI 可解释   | 玩家理解每次调整原因      |
| AI 可适应   | 玩家主动改变 Build    |
| AI 可扩展   | 新 Rule、新敌人可配置接入 |
| Demo 可展示 | 15 分钟完整体验闭环     |

---

# 21.16 Final Deliverables（最终交付物）

项目最终输出包括：

| 交付物             | 说明                       |
| --------------- | ------------------------ |
| 商业级 GDD         | 完整设计文档                   |
| UE5 工程          | 可运行 Demo                 |
| 技术设计文档（TDD）     | AI 与系统架构                 |
| 美术圣经（Art Bible） | 全套视觉规范                   |
| 数据配置表           | Rule、Enemy、Boss、Weapon 等 |
| 演示 PPT          | 比赛答辩使用                   |
| Trailer         | 1–3 分钟宣传视频               |
| Gameplay 视频     | AI Native 核心玩法展示         |

---

# 21.17 本章总结

Production Plan 为《山海镜》提供了一套完整的研发落地方案。

它明确了：

* **开发目标**（验证 AI Native 核心玩法）
* **研发流程**（Prototype → Vertical Slice → Content → Polish）
* **团队分工**（程序、美术）
* **风险管理**（AI、性能、资源、进度）
* **测试与交付**（QA、Playtest、比赛材料）
* **长期路线图**（从 MVP 到 Persistent AI）

整套计划遵循**先验证核心、后扩展内容**的原则，确保在两个月、两人团队的现实条件下，仍能完成一个具有完整 AI 闭环、可展示、可验证、可持续演进的 AI Native 游戏 Demo。

---

# Appendix A Gameplay Tags

整个游戏所有Gameplay Tag。

例如

## Character

```text
Character.Melee
Character.Range
Character.Tank
Character.Magic
Character.Support
```

---

## Weapon

```text
Weapon.Bow
Weapon.Sword
Weapon.Axe
Weapon.Mirror
Weapon.Talisman
```

---

## Skill

```text
Skill.Fire
Skill.Ice
Skill.Dash
Skill.Heal
Skill.Buff
```

---

## Enemy

```text
Enemy.Grunt
Enemy.Tank
Enemy.Rush
Enemy.Shooter
Enemy.Controller
Enemy.Support
Enemy.Elite
Enemy.Boss
```

---

## Rule

```text
Rule.Ammo
Rule.Heal
Rule.Cooldown
Rule.Crit
Rule.Resource
Rule.Environment
Rule.Special
```

---

## Build

```text
Build.Sniper

Build.Berserker

Build.Magic

Build.Tank

Build.Hybrid
```

---

整个Gameplay Tag建议

> 200+

全部配置化。

---

# Appendix B Rule Database

建议建立完整Rule数据库。

例如

| ID | Rule | Cost | Weight | Positive | Negative | Tag |
| -- | ---- | ---- | ------ | -------- | -------- | --- |

例如

```text
Rule_001

Ammo Down

Cost

20

Weight

35

Tag

Rule.Resource
```

以后

新增Rule

不用改程序。

---

# Appendix C Enemy Database

例如

| EnemyID | Threat | Tag | HP | Speed | Archetype |

例如

```text
Enemy_001

穷奇

Threat

20

Tag

Rush

HP

120
```

全部DataTable。

---

# Appendix D Passive Database

例如

| Passive | Cost | Tier | Build Tag |

例如

```text
Crit+

Rare

Sniper
```

以后AI

推荐

Passive。

就是：

读表。

---

# Appendix E Boss Database

例如

Boss

拥有

Skill Pool

Arena

Tag

Rule

等等。

以后Boss

增加

直接：

配置。

---

# Appendix F JSON Schema

例如

Director返回：

```json
{
 "PlayerModel":{},
 "ChallengePlan":{},
 "RuleModifier":[],
 "EnemyComposition":[],
 "Narration":""
}
```

统一。

Schema。

---

# Appendix G Prompt Library

这一部分。

非常重要。

真正比赛。

腾讯一定会问：

Prompt。

怎么写。

建议直接写。

例如

System Prompt。

Director Prompt。

Boss Prompt。

Narration Prompt。

Evaluation Prompt。

Reflection Prompt。

全部放这里。

---

# Appendix H UML

建议加入

整个系统UML。

例如

```text
Player

↓

Combat

↓

Recorder

↓

Mirror Mind

↓

Director

↓

Rule

↓

Encounter
```

还有：

Class Diagram。

Sequence Diagram。

State Machine。

---

# Appendix I Event List

整个EventBus。

例如

```text
OnAttack

OnHit

OnDodge

OnSkill

OnBuildChanged

OnFloorFinished

OnDirectorStart

OnDirectorFinished

OnBossSpawn

OnBossDead
```

程序。

天天看。

---

# Appendix J DataTable

整个UE5。

DataTable。

例如

```text
DT_Enemy

DT_Rule

DT_Boss

DT_Weapon

DT_Skill

DT_Passive

DT_UI

DT_Audio
```

全部。

统一。

---

# Appendix K Naming Convention

整个项目命名规范。

例如

Blueprint：

BP_

Widget：

WBP_

DataTable：

DT_

Texture：

T_

Material：

M_

Instance：

MI_

Particle：

FX_

Animation：

ABP_

Sound：

SFX_

Music：

BGM_

保证：

整个工程。

统一。

---

# Appendix L Folder Structure

UE5目录。

```
Content

├── Characters

├── Enemy

├── Boss

├── Combat

├── AI

├── Data

├── UI

├── Audio

├── Maps

├── FX

├── Animation

├── Materials

├── Blueprint
```

---

# Appendix M Version History

以后每个版本。

修改什么。

例如

Version0.1

完成：

Prototype。

Version0.2

加入：

Mirror Mind。

Version0.3

加入：

Rule。

Version1.0

比赛版。

---

# Appendix N Reference

最后。

建议加入：

论文。

书籍。

GDC。

SIGGRAPH。

Epic。

OpenAI。

Anthropic。

Riot。

Valve。

Left4Dead AI Director。

Hades。

等等。

真正商业GDD都会写。

