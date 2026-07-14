# 《山海镜》技术设计文档（TDD）v0.2

> 基于 GDD v0.2 与 Design Bible v1.0 转化
> 面向：1 程序 + 1 美术 × 8 周 MVP
> 引擎：Unreal Engine 5.5（俯视角，键鼠双摇杆瞄准 twin-stick）
> 定位：AI Native Roguelike，参加腾讯游戏开发大赛
> **Demo 目标：单局约 30 分钟可玩体验**（较 GDD 的 15 分钟翻倍，靠 AI 驱动的遭遇变化 + 模块化房间复用撑起，而非翻倍内容）
> LLM 通道：GameNPC（腾讯游戏开发大赛官方通道）
> 战斗系统：**自研轻量组件 + GameplayTags，不使用 GAS**（团队无 GAS 经验，已定）
> 编写视角：Technical Director
>
> **v0.2 变更**：① Demo 时长 15min→30min ② LLM 通道锁定 GameNPC ③ 战斗确定为 twin-stick 键鼠瞄准 ④ GAS 选型锁定为自研（不再保留 GAS 子集备选）

---

## 阅读前必读：本 TDD 的三条工程铁律

这份文档与 Design Bible 的关系：**Design Bible 是"愿景全集"（21 章、含大量正式版内容），本 TDD 只对齐 GDD v0.2 划定的 MVP，并把它拆成 2 人可执行的工程。** 凡是 Design Bible 里出现、但 MVP 不做的东西（Mirror Cognitive Architecture 五层认知、AI Economy、A/B 框架、Persistent AI 等），本文明确标注为"后续版本"，不进入 8 周排期。

三条铁律：

1. **AI 的"数学"由代码算，AI 的"策略与说话"由 LLM 做。** 平衡、预算、公平性、参数映射全部是确定性 C++；LLM 只在其上做策略选择和生成白泽台词。断网时游戏必须完整可玩。
2. **内容全部数据驱动。** 程序不写数值。美术/策划通过 DataTable + Data Asset + GameplayTag 扩展内容,不需要改代码、不需要等程序。
3. **能砍就砍。** 任何无法在 8 周内由 2 人交付、且不直接服务"AI 动态调整"这一核心卖点的功能,一律推迟。

---

# 1. 技术架构总览

## 1.1 分层架构

整个游戏分五层,上层依赖下层,同层之间只通过接口/事件通信,禁止直接引用具体类。

```text
┌─────────────────────────────────────────────────────────┐
│  表现层 Presentation (UMG / Niagara / Animation)          │
│  HUD · 导演报告 · 白泽对白 · 镜界时间轴 · 强化三选一        │
└─────────────────────────────────────────────────────────┘
                          ▲ 只读游戏状态 / 监听事件
┌─────────────────────────────────────────────────────────┐
│  玩法层 Gameplay                                          │
│  角色 · 战斗 · 武器/技能 · 敌人个体AI(BT) · 遭遇 · 关卡流程  │
└─────────────────────────────────────────────────────────┘
                          ▲ 事件上报 / 读取导演决策
┌─────────────────────────────────────────────────────────┐
│  AI 导演层 AI Director  ★核心                             │
│  行为记录器 · 画像分析器 · 导演核心 · 规则管理器 · AI服务    │
└─────────────────────────────────────────────────────────┘
                          ▲ 事件总线 / 数据表读取
┌─────────────────────────────────────────────────────────┐
│  框架层 Framework                                         │
│  事件总线 · 属性组件 · 能力组件 · 存档 · GameplayTag 注册   │
└─────────────────────────────────────────────────────────┘
                          ▲
┌─────────────────────────────────────────────────────────┐
│  数据层 Data                                              │
│  DT_Enemy · DT_Weapon · DT_Skill · DT_Passive · DT_Rule   │
│  DT_RoomTemplate · 本地兜底规则表 · Prompt 模板             │
└─────────────────────────────────────────────────────────┘
```

## 1.2 六大模块职责与交互

| 模块 | 归属层 | 职责（做什么） | 明确不做（边界） |
|---|---|---|---|
| **事件总线 EventBus** | 框架 | 全局广播玩法事件（攻击/受击/击杀/切武器/层结束）。`UGameInstanceSubsystem`。 | 不存储状态、不做业务逻辑 |
| **行为记录器 BehaviorRecorder** | AI导演 | 订阅事件,累积成"本层行为快照"`FFloorBehaviorSnapshot`。 | 不做分析、不打分 |
| **画像分析器 ProfileAnalyzer** | AI导演 | 纯确定性算法,把快照+历史→五维画像`FPlayerProfile`。 | 不调用 LLM、不决定难度 |
| **导演核心 DirectorCore** | AI导演 | 组织决策:构建 LLM 请求→拿策略→套预算/冲突/公平护栏→产出`FDirectorDecision`。 | 不直接改游戏对象 |
| **AI服务 AIService** | AI导演 | 异步 HTTP 调 LLM,收 JSON,校验;失败走本地兜底。 | 不参与实时战斗帧 |
| **关卡生成器 FloorGenerator** | 玩法 | 消费`FDirectorDecision`:设刷怪权重、全局施加规则Effect、调掉落表、组装房间。 | 不做决策,只执行 |

**一次完整交互(一层游戏的生命周期):**

```text
战斗中: Combat→EventBus→BehaviorRecorder(实时累积)
  │
层结束(OnFloorFinished)
  ▼
BehaviorRecorder 定稿快照 → ProfileAnalyzer 算出五维画像
  ▼
DirectorCore 组 Prompt → AIService 异步调 LLM ──失败──┐
  │成功                                              ▼
  │                                        LocalDirectorFallback 查兜底表
  ▼                                              │
JsonValidator 校验 ←────────────────────────────┘
  ▼
DirectorCore 套护栏(预算/冲突/公平) → FDirectorDecision
  ▼
① TransparencyUI 显示导演报告(白泽台词+调整项)   ← 玩家在这里看报告的几秒,正好掩盖 LLM 延迟
② 玩家选强化(pick 3)
  ▼
进入下一层: FloorGenerator 应用决策 → 新一层开打
```

**关键设计点:LLM 的 2 秒延迟被"导演报告 + 强化选择"界面自然掩盖,不阻塞任何战斗帧。**

## 1.3 数据通信流程

- **模块内/进程内**:事件总线(广播)+ 直接接口调用(查询)。
- **与 LLM**:HTTPS,JSON 请求/响应,每层一次,异步。**走 GameNPC(比赛官方通道)**;接口抽象成`IAIProvider`,`FGameNPCProvider`是正式实现,开发期可切一个 OpenAI 兼容的`FDebugProvider`本地联调,避免频繁占用官方额度。**务必尽早跑通 GameNPC 的鉴权 + 一次真实往返**(填进 W6 前的技术预研,别拖到最后),把它的 QPS 限制、超时表现、返回格式摸清楚,直接决定 §16 兜底策略的触发阈值。
- **存档**:三层——Runtime(当前 Run)、Mirror(跨 Run 的画像历史,MVP 只存最近若干局的简版)、Player(解锁,MVP 可空)。本地`USaveGame`,异步写。

---

# 2. 核心 Gameplay 系统设计

## 2.1 玩家系统

**俯视角 twin-stick 键鼠瞄准**:WASD 移动,鼠标位置决定瞄准方向(角色始终朝鼠标),左键攻击、右键/Shift 闪避、Q/E 技能、空格/滚轮切武器。角色朝向与移动方向解耦(可以边后退边射击),这是远程"风筝"手感的基础,也让 AI 的"移动压力"规则(逼近/打断)真正能改变体验。玩家 Pawn = `ASHMCharacter`(C++ 基类),具体角色是它的蓝图子类。

> 瞄准解耦对战斗组件的影响:攻击方向取"鼠标世界坐标 - 角色坐标"的朝向,而非角色 forward;远程投射物、近战扇形判定都用这个瞄准向量。输入用 Enhanced Input,`Aim`轴每帧从鼠标射线更新。

```cpp
// ASHMCharacter：所有可玩角色基类（C++）
UCLASS()
class ASHMCharacter : public ACharacter
{
    UPROPERTY() USHMAttributeComponent* Attributes;   // 血/能量/移速等
    UPROPERTY() USHMAbilityComponent*   Abilities;    // 2 技能槽 + 闪避
    UPROPERTY() USHMWeaponComponent*    WeaponComp;    // 主副武器切换
    UPROPERTY() USHMBuildComponent*     Build;         // 被动强化叠加

    // 输入接口（Enhanced Input）
    void Move(FVector2D); void Aim(FVector2D);
    void LightAttack(); void Dodge(); void CastSkill(int32 Slot); void SwitchWeapon();
};
```

**边界**:角色类只管"响应输入 + 持有组件",不写具体武器逻辑(在 WeaponComp)、不写数值(在 DataTable)。
**扩展方式**:新角色 = 新蓝图子类 + DataTable 一行(起始武器/技能/属性),不动 C++。

## 2.2 战斗系统

自研轻量组件,不上完整 GAS(理由见 5.2)。三根支柱:

- **属性 `USHMAttributeComponent`**:一组`FSHMAttribute{ Base, BonusFlat, BonusPct, Current }`,支持"最终值 = (Base+Flat)×(1+Pct)"。规则修改器和被动强化都通过往这里加/减 Bonus 生效。
- **能力 `USHMAbilityComponent`**:技能/闪避的冷却、前摇、消耗、命中判定。技能定义在`UAbilityDataAsset`。
- **命中反馈**:Hit Stop(仅暂停攻击者动画)、削韧(Poise)、暴击。数值全走 DataTable。

**削韧(Poise)**:敌人有韧性值,重击削韧,削空进入硬直窗口。这是"近战爽感"的关键,必须做。

**攻击判定**:MVP 用简单的碰撞/球形 Overlap + 帧窗口,不做复杂的 hitbox 骨骼绑定(那是打磨期加分,不是 MVP 必需)。

## 2.3 角色系统(MVP:2 个,第 3 个作加分)

| | 力士(近战) | 方士(远程) |
|---|---|---|
| 定位 | 高血、削韧、弹反 | 走位、法术消耗 |
| 起始武器 | 重剑/长刀 | 猎弓/卦镜 |
| AI 画像标签 | `Archetype.Vanguard` | `Archetype.Ranger` |

角色只决定"起点",不决定"终点"——通过 Build 组件可以打出多种流派。

## 2.4 武器/技能系统

全部数据驱动,武器是数据 + 一个攻击模式枚举,不是每把一个类。

```cpp
USTRUCT() struct FWeaponRow : public FTableRowBase
{
    FGameplayTag       WeaponTag;      // Weapon.Bow / Weapon.Sword ...
    EAttackPattern     Pattern;        // 连射 / 三段重击 / 法术穿透
    float              BaseDamage, AttackSpeed, Range, PoiseDamage;
    FGameplayTagContainer BuildTags;   // 参与 Build 识别: Ranged/Crit...
    TSoftObjectPtr<UAnimMontage> AttackMontage;
    TSubclassOf<AProjectile>     ProjectileClass; // 远程才有
};
```

**关键:武器/技能都带 `GameplayTag`,这是 AI Director 识别"玩家在用什么打法"的唯一依据——AI 读 Tag,不读名字。** 这是整个 AI 系统能工作的地基。

技能采用"替换式"两槽(GDD 设定),换技能立即改变打法。技能定义在`UAbilityDataAsset`(Data Asset 而非 DataTable,因为技能含逻辑蓝图引用)。

## 2.5 敌人个体 AI 系统(注意与"AI 导演"区分)

> ⚠️ 命名澄清:**敌人个体 AI = 行为树 BT**(单只怪怎么追、怎么打);**游戏导演 AI = AI Director 子系统**(整层怎么配、改什么规则)。两者完全不同,不要混。

敌人 = `ASHMEnemy`(C++ 基类) + `UBehaviorTree` + `EnemyDataAsset`。所有敌人**共享一棵行为树**,差异只在"攻击节点"和数据。

```cpp
USTRUCT() struct FEnemyRow : public FTableRowBase
{
    FGameplayTag          ArchetypeTag;  // Enemy.Tank/Rush/Shooter/Controller/Support/Elite/Grunt
    int32                 ThreatValue;   // 遭遇预算用
    float                 HP, Speed, Poise, ContactDamage;
    TSubclassOf<ASHMEnemy> EnemyClass;   // 具体表现蓝图
    FGameplayTagContainer  CounterTags;  // 克制谁: 克制 Build.Ranged 等
};
```

**7 类原型**(对齐 Design Bible ch10):Grunt / Tank / Rush / Shooter / Controller / Support / Elite。MVP 做满 10-15 种"具体异兽",但它们只是这 7 个原型的皮肤 + 数值变体。

**AI Director 永远只输出原型标签的权重(如 `Enemy.Tank: 0.3`),由生成器从该原型的怪池里随机抽具体怪。** 这保证:①重复游玩有变化 ②新增异兽不动 AI 代码。

## 2.6 Roguelike 随机事件 / 强化系统

- **每层清怪后 pick 3 选 1**(经典肉鸽)。强化池来自`DT_Passive`。
- **被动强化**:可叠加,通过给属性组件加 Bonus 实现。带`BuildTags`供 AI 识别流派。
- **AI 诱导(非强制)**:AI Director 可对三选一的**权重**施加影响(如玩家一直远程,提高近战强化出现概率),但**绝不直接送强化、绝不锁死选择**——这是"AI 是老师不是惩罚者"的工程体现。
- **MVP 强化数量**:~15 个,覆盖攻/防/暴击/冷却/近战增伤/远程增伤/击杀回血等。

## 2.7 关卡生成系统(重要:定义"生成"的边界)

**MVP 的"生成"= 结构生成 + 内容填充,不含几何生成。**

- **结构**:Slay the Spire 式节点图。一个 Run = **6 层**,每层 **3-4 个房间节点**(战斗/精英/事件/Boss)。节点图由代码按简单规则生成(层数固定 6,每层房间数固定)。
- **几何**:房间布局由**美术手搓成 6-8 个房间模板**(Level Instance / 预制场景),存`DT_RoomTemplate`,带标签(开阔/狭窄/多掩体)。生成器按 AI Director 的战场偏好(远程玩家→多掩体房)选模板。**模板复用是关键**:6-8 个模板 × AI 不同的刷怪/规则组合 = 几十种不重样的战斗体验,art 不用线性增长。
- **内容**:`EncounterManager`按 AI Director 的敌人权重 + 遭遇预算(Threat Budget)刷怪,分波次(Wave)。

### 时长与节奏结构(30 分钟 Demo 的骨架)

| 层 | 房间数 | 时长 | AI Director 行为 | 演示看点 |
|---|---|---|---|---|
| F1 | 3 | ~4min | 只观察,不调整(建立画像) | 教玩家操作 |
| F2 | 3 | ~4min | 首次轻度调整 + 首张导演报告 | 玩家第一次感到"被观察" |
| F3 | 4 | ~5min | **精英遭遇(中途 checkpoint)** + 中度调整 | 节奏高点,压力测试 Build |
| F4 | 4 | ~5min | 定向反制 + 规则加深 | 玩家被逼换打法 |
| F5 | 4 | ~5min | 高压 + 战场偏好介入 | 对抗最激烈 |
| F6 | Boss | ~5min | 烛龙(读画像选 1 动态技能,战中不再改) | 毕业考试 |

**单局 ≈ 28-32 分钟**,达标。节奏是波浪形(F3 精英、F6 Boss 为两个高点,中间有喘息),对齐 Design Bible 的 Pressure Wave。

**30 分钟从哪来 —— 三个"内容放大器"(而非翻倍内容)**:
1. **AI 遭遇变化**:同一批 10-15 种怪,AI 按画像重组权重/波次/战场,每层都不一样。这是核心卖点顺手带来的时长红利。
2. **模块化房间复用**:6-8 个模板反复用,靠 AI 的遭遇差异让玩家感觉"新"。
3. **多样性反馈激励留客**:玩家被 AI 逼着换 Build,单局体验的"密度"上升,自然拉长有效游玩时间。

**⚠️ 30 分钟的真实代价(必须正视)**:压力几乎全落在 **美术(多 2 个房间模板 + 精英怪表现)** 和 **调平衡(6 层难度曲线、遭遇预算)** 上,核心系统代码几乎不增加。这条写进第 6 节排期和风险里。若 W7 发现调不动,**保底退回 5 层 ~24 分钟**,并用"鼓励第二局看 AI 跨局适应"补足到 30 分钟体验(见 §6 保底策略)。

```cpp
// FloorGenerator 消费导演决策的核心接口
void UFloorGenerator::BuildFloor(const FDirectorDecision& Decision, int32 FloorIndex)
{
    ApplyGlobalRules(Decision.RuleModifiers);       // 全局 GameplayEffect
    AdjustLootTable(Decision.ResourceAdjust);       // 掉落表
    for (FRoomNode& Room : GenerateRoomGraph(FloorIndex))
        EncounterManager->Configure(Room, Decision.EnemyWeights, ThreatBudget(FloorIndex));
}
```

**❌ 不做**:运行时程序化生成房间几何(Wave Function Collapse 那类)。对 2 人 8 周是死亡陷阱,且对"AI 动态调整"卖点零贡献。

---

# 3. AI Director 系统设计 ★核心

这是全项目技术核心,详细展开。核心闭环:**观察 → 画像 → 决策 → 应用 → 解释**。

## 3.1 第一步:收集玩家行为数据

通过事件总线采集,`BehaviorRecorder`累积成"层快照"。14 项埋点(对齐 GDD 6.4):

```cpp
USTRUCT(BlueprintType)
struct FFloorBehaviorSnapshot
{
    int32 FloorIndex = 0;

    // 维度1 Build集中度
    TMap<FGameplayTag, int32> WeaponAttackCounts;   // Weapon.Bow -> 120
    TMap<FGameplayTag, int32> SkillCastCounts;      // Skill.Fire -> 8
    TMap<FGameplayTag, float> DamageByTag;          // 各武器/技能贡献伤害
    TMap<FGameplayTag, int32> KillsByTag;

    // 维度2 战斗效率
    float RoomClearTimeAvg = 0.f;
    int32 HitsTaken = 0;
    float TotalDamageTaken = 0.f;
    float DodgeSuccessRate = 0.f;

    // 维度3 资源盈余
    int32 HealItemsRemaining = 0;
    int32 AmmoRemaining = 0;

    // 维度4 策略切换
    int32 WeaponSwitchCount = 0;
    TSet<FGameplayTag> UniqueSkillsUsed;
    float BuildDiffFromLastFloor = 0.f;             // 与上层 Build 差异度

    // 维度5 生存压力
    int32 LowHpEvents = 0;                          // HP<20% 触发次数
    int32 DeathOrNearDeath = 0;
};
```

采集事件清单(EventBus 广播,Recorder 订阅):
`OnAttack(WeaponTag, Damage)` · `OnHitTaken(Src, Damage)` · `OnDodge(bSuccess)` · `OnSkillCast(SkillTag)` · `OnKill(EnemyTag, ByTag)` · `OnWeaponSwitch(From,To)` · `OnItemUse(Tag)` · `OnLowHp()` · `OnRoomFinished(Time)` · `OnFloorFinished()`

## 3.2 第二步:建立玩家画像(纯确定性,不碰 LLM)

`ProfileAnalyzer`把快照 + 历史 → 五维 0-100 画像。**这部分是纯 C++ 算法,可单元测试,断网也能跑。**

```cpp
USTRUCT(BlueprintType)
struct FPlayerProfile
{
    float BuildConcentration = 0;   // 0-100,单一打法占比越高越大
    float CombatEfficiency   = 0;
    float ResourceSurplus    = 0;
    float StrategySwitch     = 0;
    float SurvivalPressure   = 0;

    FGameplayTag        DominantArchetype;   // Archetype.Ranger 等
    TArray<FGameplayTag> PrimaryBuildTags;   // 主力打法标签,喂给敌人克制
    float Confidence = 0.5f;                 // 对判断的置信度(简版 Mirror Mind)
};
```

**算法示例(Build 集中度)**:

```text
总攻击次数 = Σ WeaponAttackCounts
最大单武器占比 = max(单武器次数) / 总攻击次数
BuildConcentration = clamp(最大单武器占比 × 100, 0, 100)
若 BuildConcentration > 85 → 该武器 Tag 进 PrimaryBuildTags
```

**置信度(简化的 Mirror Mind)**:不搞 Design Bible 那套五层认知。MVP 只做一个简单规则——**连续 N 层同一打法才提高置信度,置信度低时 AI 不激进针对**。这一条就能避免"玩家临时换了把武器就被误判针对"的体验问题,成本极低、效果明显。

```text
若本层 DominantArchetype == 上层 → Confidence += 0.2 (上限 0.95)
否则 → Confidence = 0.5 (重新观察)
Confidence < 0.6 时,DirectorCore 只允许轻度调整
```

**玩家原型(Archetype)**:由主力标签映射,MVP 做 4 类够用:Ranger(游击)/ Vanguard(先锋)/ Berserker(狂战)/ Survivor(幸存者)。

## 3.3 第三步:根据画像生成调整策略

以你给的例子走一遍完整链路:

```text
输入画像:
  远程攻击占比 90% → BuildConcentration=90, DominantArchetype=Ranger
  近战几乎不用    → PrimaryBuildTags=[Build.Ranged]
  受击少          → SurvivalPressure=10, CombatEfficiency=88
  Confidence=0.9(连续 3 层远程)

DirectorCore 决策(挑战等级 Level 2 Pressure):
  ① 敌人:提高克制 Ranged 的原型权重
       Enemy.Tank +0.3(逼近压缩输出空间)
       Enemy.Rush +0.2(打断站桩)
  ② 规则:弹药掉落 -25%(Medium),移动压力↑
  ③ 资源:回复道具正常(因为生存压力低,不额外收)
  ④ 战场:多掩体房权重↓(不给远程站桩点)
  ⑤ 白泽台词:"你的弓用得很好。但这一层,别指望站在原地。"

护栏检查:
  预算 Floor3=55,已用 Tank20+Rush15+弹药20=55 ✓
  冲突检查:弹药↓ 未与"远程伤害↓"同时出现 ✓(否则无解)
  公平检查:弹药↓ 未连续 3 层出现 ✓
```

## 3.4 AI 输入 / 输出格式(LLM 接口契约)

**输入(DirectorCore → LLM)**:

```json
{
  "player_profile": {
    "build_concentration": 90, "combat_efficiency": 88,
    "resource_surplus": 70, "strategy_switch": 12, "survival_pressure": 10,
    "archetype": "Ranger", "primary_build": ["Build.Ranged"], "confidence": 0.9
  },
  "context": { "floor": 3, "total_floors": 5, "character": "方士",
               "challenge_budget": 55 },
  "history": [ {"floor":2,"rules":["Ammo-20%"],"player_adapted":false} ],
  "available_rules": [
    {"tag":"Rule.Ammo","cost":20,"level":"medium"},
    {"tag":"Rule.Cooldown","cost":15,"level":"light"},
    {"tag":"Rule.Heal","cost":20,"level":"medium"}
  ],
  "available_enemy_archetypes": ["Enemy.Tank","Enemy.Rush","Enemy.Shooter","..."]
}
```

**输出(LLM → DirectorCore,严格 JSON Schema)**:

```json
{
  "challenge_level": 2,
  "enemy_weights": { "Enemy.Tank": 0.3, "Enemy.Rush": 0.2, "Enemy.Grunt": 0.5 },
  "rule_modifiers": [ { "tag": "Rule.Ammo", "level": "medium" } ],
  "resource_adjust": { "Resource.Heal": 0.0 },
  "arena_preference": "open",
  "narration": "你的弓用得很好。但这一层,别指望站在原地。",
  "reason": "玩家连续三层远程站桩,高置信度,压缩输出空间并施加移动压力。"
}
```

**注意 LLM 输出里没有任何具体数值(如"弹药-25%")。** LLM 只给标签和等级,**具体数值由 C++ 从`DT_Rule`查表映射**(`Rule.Ammo + medium → ×0.75`)。这样策划改数值不用碰 Prompt,LLM 也无法破坏平衡。

## 3.5 哪些交给规则、哪些交给 LLM(最关键的分工)

| 职责 | 由谁做 | 为什么 |
|---|---|---|
| 行为→五维画像 | **规则(C++)** | 必须确定、可测、可离线 |
| 预算/冲突/公平护栏 | **规则(C++)** | 平衡安全护栏,绝不能让 LLM 破坏 |
| 标签→具体数值映射 | **规则(C++查表)** | 策划可调,LLM 不接触数值 |
| API 失败兜底 | **规则(C++)** | 断网必须能玩 |
| **针对哪个维度、选哪些规则组合** | **LLM** | 有多个合法解时,LLM 做"有品味的选择",这是可变、难以穷举的部分 |
| **白泽台词/解释文案** | **LLM** | 自然语言个性化,规则表写不出的东西——这是 AI Native 最可见的证据 |
| **跨局反思(可选加分)** | **LLM** | "上层策略是否奏效"的语言化判断 |

**一句话原则:LLM 负责"选择与表达",代码负责"计算与安全"。** LLM 挂了,游戏用兜底表照常运行,只是白泽的话变成模板句、策略变保守。

---

# 4. AI 实现方案对比与推荐

| | 方案A 纯规则 | 方案B 规则+LLM ✅ | 方案C LLM Agent 当导演 |
|---|---|---|---|
| **开发成本** | 低(2-3 周) | 中(3-4 周,LLM 是薄层) | 高(需 agent 编排、工具调用、大量 prompt 调试) |
| **稳定性** | 极高 | 高(LLM 只在层间、有兜底) | 低(实时/多轮易超时、跑飞、JSON 不合法) |
| **展示效果** | 弱——评委会说"这不就是 DDA/权重表" | **强——白泽能用自然语言解释每次决策,肉眼可见"AI 在思考"** | 理论最炫,但演示时最容易翻车(延迟、胡言乱语) |
| **比赛可行性** | 完成度高但创新分低 | **完成度与创新分平衡最优** | 完成度风险极高,2 人 8 周难压稳 |
| **API 成本** | 0 | 低(每层 1 次,~5-8 次/局) | 高(多轮 + 工具调用) |
| **"AI Native"说服力** | 不足 | **足够(见第 8 节论证)** | 足够但赌完成度 |

**推荐:方案 B。** 理由:

1. 方案 A 的画像+权重逻辑其实是方案 B 的地基,**你无论如何都要先做出 A**。方案 B = A + 一个可插拔的 LLM 薄层。这意味着**即使 LLM 集成失败或比赛现场断网,你手里仍有一个完整可玩的方案 A 兜底**。这是 2 人团队最需要的"下限保障"。
2. LLM 的价值集中在**两个规则表写不出的地方**:(a)在多个合法策略中做有品味的、随画像连续变化的选择;(b)生成白泽的个性化自然语言解释。这两点恰好是"AI Native"最可见的证据,且是方案 A 的天花板。
3. 方案 C 的实时 agent 对**动作 Roguelike**是负收益——战斗中没人等 LLM,层间的决策用一次性调用足够,多轮 agent 的复杂度纯属自找麻烦。**把 agent 的想法留给 Design Bible 说的 V3.0 Persistent AI。**

**落地形态:方案 B,但按"先 A 后 B"实施——第一阶段先做纯本地导演(方案A),第二阶段把 LLM 作为可开关的增强层叠上去。** LLM 从第一天起就是"可拔掉的"。

---

# 5. Unreal Engine 实现方案

## 5.1 C++ / 蓝图划分

**原则:骨架、系统、性能敏感、需单测的 → C++;内容、表现、调参、迭代频繁的 → 蓝图。**

| C++(程序 A 负责) | 蓝图(美术 B 可独立做) |
|---|---|
| EventBus、BehaviorRecorder、ProfileAnalyzer、DirectorCore、AIService(HTTP+JSON)、RuleManager | 具体敌人 BP(继承 C++ 基类)、VFX、动画蓝图 ABP |
| 属性/能力/武器/Build 组件 | UI 控件 WBP(导演报告/HUD/三选一) |
| ASHMCharacter / ASHMEnemy / AProjectile 基类 | 房间关卡、掉落物摆放 |
| FloorGenerator / EncounterManager | 具体技能的表现层(前摇特效/音效挂点) |
| 所有 DataTable 的行结构体、GameplayTag 注册 | 填 DataTable 的每一行数值 |

## 5.2 是否用 GAS?——**不用完整 GAS,用轻量自研 + GameplayTags**

这是一个我要明确给结论的选型:

- **GAS 的优点确实契合本项目**:GameplayEffect 天然适合 Rule Modifier(临时全局 buff/debuff),GameplayTag 是你们整个 AI 识别体系的基础。
- **但 GAS 的学习曲线对"1 名程序 8 周"是重大风险**:ASC 复制、Attribute Set、GE 执行链、预测……这些为多人联机设计的复杂度,你们单机俯视角完全用不到,却要全交学费。

**结论(已锁定):采纳 GAS 的思想,不采纳 GAS 的实现。团队无 GAS 经验,本项目不引入 GAS,不保留 GAS 子集备选。**
- ✅ **全量使用 GameplayTags**(免费、简单、是你们 AI 的地基,Design Bible ch16 也强制要求)。这一项和 GAS 无关,单独就能用。
- ✅ 自研 `USHMAttributeComponent`(Base/Flat/Pct 三段)+ `USHMEffectComponent`(临时修改器,带时长/来源),用它实现规则修改器和被动。这套东西 1 名程序 3-4 天能写完并可控,且行为完全可预测、可单测——对"规则修改器不能破坏平衡"这条铁律至关重要。
- ✅ 技能命中/冷却/前摇放在 `USHMAbilityComponent`,不走 GA。表现层(蒙太奇、特效)由蓝图挂。

**为什么这对你们是正确的**:GAS 的价值在"大量技能 + 联机预测回滚"的场景才回本;你们是**单机、俯视角、技能数量有限**,自研组件的总成本远低于学 GAS 的成本,而且规则系统需要的"确定性、可测试、断网可跑"恰恰是自研最容易保证、GAS 最难保证的。

## 5.3 敌人 AI Controller 设计

- `ASHMEnemyController : AAIController`,持有一棵**共享行为树** + Blackboard。
- BT 结构:`是否发现玩家 → 距离判断 → (近)攻击 / (远)追击 / (Shooter)保持距离射击`。
- 差异化靠**Data Asset 参数 + 攻击子树**,不是每类怪一棵树。
- 感知用`AIPerception`(视觉),简单够用。

## 5.4 DataTable / Data Asset 划分

- **DataTable(表格型、纯数值、策划批量填)**:`DT_Enemy` `DT_Weapon` `DT_Skill` `DT_Passive` `DT_Rule` `DT_RoomTemplate` + `DT_LocalDirectorFallback`(兜底规则表)。
- **Data Asset(含资源引用/逻辑,单个配置)**:`UAbilityDataAsset`(技能)、`UEnemyArchetypeAsset`(原型行为配置)、`UCharacterDataAsset`(角色起始配置)。

## 5.5 关卡生成方式

见 2.7:**节点图(代码生成) + 房间模板(美术手搓) + 遭遇(AI 权重刷怪)**。不做运行时几何生成。

## 5.6 如何快速做出 MVP

- **先做"一个房间能打怪"的垂直切片**,把战斗手感磨到及格,再谈系统。
- **AI 先做纯本地导演(方案A)**,让"层间可见调整"先跑起来,LLM 后叠。
- **一切占位优先**:美术资源用 UE 商城/占位模型,程序不等美术;数值先拍脑袋填 DataTable,后调。
- **每周五出一个能从头玩到当前进度的构建**(对齐 Design Bible ch21 "Playable Every Week")。

---

# 6. MVP 优先级规划(8 周)

> 现状(据 git):UE5.5 工程已建、俯视角移动基础框架已有、纯英文路径已迁移、编译通过。从"战斗垂直切片"接着往下做。

## 第一阶段(第 1-4 周):核心闭环必须完成 —— 交付"方案A 可玩"

| 周 | 产出 | 验收 |
|---|---|---|
| W1 | 战斗切片:1 角色(力士)、轻攻击、闪避(无敌帧)、削韧、Hit Stop | 一个房间能爽快打怪 |
| W2 | 3-4 种敌人(Grunt/Tank/Rush/Shooter)+ 共享 BT + 遭遇刷怪 + 波次 | 一个房间有节奏地打完 |
| W3 | EventBus + BehaviorRecorder + ProfileAnalyzer(五维画像) | 打印出正确的画像数据 |
| W4 | **本地 DirectorCore(方案A)** + 规则/预算/冲突 + FloorGenerator + 层过渡 + 三选一 + 导演报告 UI(占位) | **能连打 2-3 层,肉眼看到 AI 按画像调整刷怪和规则** |

**第一阶段结束 = 一个没有 LLM、但"AI 动态调整"已成立的可玩 Demo。这是你们的安全下限。**

## 第二阶段(第 5-6 周):内容 + LLM,提升完成度

| 周 | 产出 |
|---|---|
| W5 | 第 2 角色(方士)、敌人补到 10-15 种、**6 层完整流程(含 F3 精英点)**、~15 强化、~15 规则、**6-8 房间模板** |
| W6 | **GameNPC 技术预研跑通** → AIService 接入 LLM(可开关) + JSON 校验 + 三级兜底、白泽台词由 LLM 生成、烛龙 Boss(3 阶段、1 个动态技能) |

> **30 分钟的负载提醒**:第二阶段是 30min 目标的主战场,压力集中在美术(房间模板 + 精英怪表现)和调平衡(6 层曲线)。**W5 美术任务必须并行提前启动**——F1 能玩时,美术就该在做 F3-F6 的模板和精英怪,不要等系统全好才动。

## 第三阶段(第 7-8 周):打磨 + 比赛加分

| 周 | 产出 |
|---|---|
| W7 | 导演报告精修、**镜界时间轴(整局 AI 决策回放,超强演示素材)**、白泽视觉/音效反馈、简版跨局记忆、**6 层难度曲线调平衡** |
| W8 | 兜底鲁棒性、平衡调参、Bug、录制 Gameplay 视频 + Trailer、比赛材料 |

**加分项(有余力才做,排序)**:①"关掉 AI Director"对照开关(演示杀器,见第 8 节) ②镜界时间轴 ③简版跨局记忆(**同时是 30min 保底手段,优先级因此上调**) ④第 3 角色 ⑤LLM 反思。

## 30 分钟达标策略与保底

- **主路径(目标)**:6 层单局 ≈ 28-32 分钟,一局玩满即达标。
- **保底路径(W7 若发现 6 层调不动/内容不够)**:退回 **5 层 ≈ 24 分钟** + **简版跨局记忆**——演示时打第二局,白泽用"你上局也靠弓,这次我提前布置好了"体现跨 Run 学习,两局合计轻松 30min+,而且**跨局适应本身是更强的 AI Native 卖点**,退这一步不亏反赚。因此把"简版跨局记忆"从纯加分项提到"保底储备"。
- **红线**:无论哪条路径,**单局核心闭环(观察→画像→决策→应用→解释)必须在 W4 结束时已成立**。时长是内容问题,不能反过来拖垮核心系统。

## ❌ 看似酷但必须放弃(写死在文档里,防止跑偏)

| 放弃项 | 原因 |
|---|---|
| 运行时程序化地形生成 | 2 人 8 周做不完,对核心卖点零贡献 |
| 完整 GAS(联机复制) | 学习曲线吞掉工期,单机用不到 |
| 实时 LLM(战斗中调) | 延迟毁掉动作手感,且无必要 |
| Design Bible 的五层认知架构 / AI Economy / A/B 框架 | 正式版内容,MVP 一个简单置信度规则就够 |
| 元素系统、商店、角色进化 | GDD v0.2 已明确砍掉 |
| 白泽全程配音 | 纯文字即可,配音是打磨末位 |
| 多 Boss | MVP 单 Boss(烛龙) |

---

# 7. 两人协作开发规范

## 7.1 核心难题:UE 二进制资产不能合并

`.uasset`/`.umap` 是二进制,**两人同时改同一个文件 = 必冲突且无法合并**。解决方案:

- **Git + Git LFS**(2 人规模够用,不必上 Perforce)。所有 `.uasset .umap` 走 LFS。
- **文件锁纪律 + 所有权分区**:靠约定而非工具强制(2 人成本最低)。

## 7.2 所有权分区(减少互相阻塞的根本手段)

| 目录 | 归属 | 说明 |
|---|---|---|
| `/Source/**` | 程序 A 独占 | 美术不碰 |
| `/Content/Core, /AI, /Data(DataTable)` | 程序 A 主导 | 结构由 A 定,数值 B 可填 |
| `/Content/Characters, /Enemy, /Boss, /FX, /UI, /Maps` | 美术 B 独占 | 程序不碰美术资产 |
| GameplayTag 注册表 | 程序 A 维护,变更通知 B | 这是双方的"接口" |

**关键:接口契约 = C++ 基类 + DataTable 行结构 + GameplayTag 注册表。** 只要契约稳定,美术 B 做 BP 子类、填 DataTable、摆房间,**完全不需要等程序 A**;程序 A 写系统,**完全不需要等美术资源**(用占位)。这就是"数据驱动"在协作上的真正价值。

## 7.3 Git 工作流(2 人极简)

- **主干开发 + 短特性分支**。2 人不搞复杂 GitFlow。
- **每日必做**:开工先 `git pull`;改 Map 前在群里喊一声"我锁 XX 图"(口头锁)。
- **提交粒度小、频繁**。每周五合并出周构建。
- **`.gitignore`(UE 必备)**:
  ```
  Binaries/ Intermediate/ Saved/ DerivedDataCache/ .vs/ *.sln
  ```
- **`.gitattributes`(LFS)**:
  ```
  *.uasset filter=lfs diff=lfs merge=lfs -text
  *.umap   filter=lfs diff=lfs merge=lfs -text
  *.fbx *.png *.wav *.mp4 filter=lfs diff=lfs merge=lfs -text
  ```

## 7.4 减少阻塞的接口约定

1. **新内容 = 加数据行,不改代码**。加个敌人 = `DT_Enemy` 加一行 + 一个 BP 子类。
2. **GameplayTag 先行**。任何新武器/敌人/规则,先在注册表加 Tag,双方以 Tag 通信。
3. **DirectorDecision 是玩法层与 AI 层的唯一接口**。玩法层只认这个结构体,不关心它来自 LLM 还是兜底表。
4. **每个系统留 Debug 面板**(ImGui 或 UE 的 CheatManager):能手动灌画像、强制指定导演决策,让美术无需真打就能测内容。

---

# 8. 评委视角审查

以比赛评委的挑剔眼光自审,并给出加固方案。

## 8.1 这个 AI 玩法是否真正体现 AI Native?

**基本成立,但需要主动证明。** AI Native 的判据是"抽掉 AI,核心体验是否坍塌"。本设计里,**玩家画像→定向反制→逼迫换 Build 的对抗循环就是游戏本身**,不是附加难度条。抽掉 Director,它退化成一个普通的固定难度刷怪 Roguelike——体验明显坍塌。这一点站得住。

## 8.2 有没有"为了 AI 而 AI"?——**最大的风险点,要正面回应**

**诚实说:方案 B 里,画像+权重那部分,一个纯规则引擎(方案A)就能做到 ~70%。评委完全可能问:"你这个 LLM 到底带来了什么规则表带不来的东西?"**

必须让 LLM 的不可替代性**可见**,否则真的会被打成"为了 AI 而 AI":

1. **自然语言解释(白泽)**:LLM 针对具体画像生成的、每局都不同的解释与吐槽,是权重表根本产不出的。这是 LLM 价值最直接的可见证据。
2. **策略组合的涌现**:规则表是"if 远程 then 加盾兵"的硬映射;LLM 能在几十条规则/多个原型里做**上下文相关的组合选择**,且随画像连续变化。演示时展示"同样是远程玩家,站桩型和风筝型拿到不同的针对方案"。
3. **跨局记忆的语言化**(加分):"你上一局也是靠弓,这次我提前准备好了"——这种跨 Run 的、有记忆感的表达,是 AI Native 的高光。

## 8.3 最容易被评委质疑的地方

| 质疑 | 加固方案 |
|---|---|
| "AI 调整我根本没感觉到" | **导演报告 + 白泽台词把每次调整显式告知**(Design Bible 的 Transparency 就是为这个);演示时强调"透明性"是刻意设计 |
| "这跟《求生之路》AI Director / 传统 DDA 有啥区别" | 明确对比:传统 DDA 改**数值**(敌人变肉),我们改**规则与打法结构**且**定向针对具体 Build**,并**用语言解释原因** |
| "LLM 是不是噱头,断网就废" | 展示**兜底机制**:现场关网络,游戏照常跑(方案A),证明架构工程成熟度反而是加分 |
| "怎么证明 AI 真在针对我而不是随机" | **"关掉 Director"对照开关** + **镜界时间轴回放**每层决策与原因 |
| 延迟/成本 | 说明每层 1 次异步调用、被报告界面掩盖、有缓存 |
| "30 分钟但内容就 10 来种怪、几个房间,是不是很单薄" | 正面回应:**这正是 AI Native 的价值——用有限内容通过 AI 重组产生 30min 不重样体验**。开/关 Director 对照 + 镜界时间轴证明"30 分钟的变化来自 AI 而非堆量",把"内容放大"讲成技术主张而非遮短 |

## 8.4 如何增强技术亮点(给答辩用)

1. **"AI Director 开/关"对照演示**——同一角色同一打法,开 Director 被定向反制、关 Director 平铺直叙。**这是最有说服力的一张牌,务必做。**
2. **镜界时间轴**——一整局把 AI"看到什么→判断什么→改了什么→玩家如何回应"串成一条可回放的故事线。既是玩法反馈,也是最好的答辩素材。
3. **透明性作为核心设计而非补丁**——强调"我们刻意让 AI 可见、可解释",把"玩家能理解 AI"作为技术主张。
4. **工程成熟度**——三级兜底、JSON 校验、数据驱动、标签化架构,证明这不是 hack 出来的 demo,而是可持续演进的框架(呼应 Design Bible 的版本路线图)。
5. **量化指标**——哪怕只做两三个:Build 多样性指数、策略适应率、"玩家认为 AI 公平/作弊"的测试比例。有数据的答辩碾压纯讲故事。

## 8.5 一句话总结给答辩

> 传统游戏里 AI 服务于游戏;《山海镜》里 **AI 理解玩家、再设计游戏**。我们用"规则引擎保证公平与稳定、LLM 负责理解与表达"的混合架构,让这套 AI 既炫、又稳、还能断网可玩——这是一个能演示、能落地、能持续演进的 AI Native 框架,而不是一个赌运气的 LLM 玩具。

---

## 附:与 Design Bible 的对应关系(便于查阅)

| 本 TDD | 对应 Design Bible 章节 | MVP 取舍 |
|---|---|---|
| §3 AI Director | ch5 AI Director / ch7 Adaptive Challenge | 保留核心闭环,砍 Challenge Heat Map 等 |
| §3.2 置信度 | ch12 Mirror Cognitive Architecture | **大砍**:五层认知→一条置信度规则 |
| §2.5 敌人 | ch10 Enemy Framework | 保留 7 原型 + Tag + Threat |
| 规则系统 | ch13 Rule Modifier Framework | 保留 5 层规则 + 预算 + 冲突,砍环境/特殊规则 |
| §5.2 技术 | ch16 Technical Architecture | 保留 EventBus + Tag + DataTable,**不上完整 GAS** |
| §8 透明性 | ch15 AI Transparency Framework | 保留导演报告 + 白泽 + 时间轴 |
| §6 排期 | ch21 Production Plan | 对齐 8 周里程碑 |
```

