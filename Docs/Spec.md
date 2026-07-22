> ⚠️ **设计存档（2026-07-21 之前）。** 项目已重新定位为**求职作品集项目**，实际范围以 [`DECISIONS.md`](./DECISIONS.md) 为准。
> **仍然有效**：S1（已完成入库）、S2-F1/F2/F3/F4、S3 全部（AI Director 本地版）、S6-F1 的对照开关部分。
> **已 OUT OF SCOPE**：S2-F5 房间模板（D-10）、S4 全部（Pick3 / 方士 / 6 层 / Boss，D-09/D-03/D-01/D-04）、S5-F1 的 GameNPC 通道（D-08，改 OpenAI 兼容）、S5-F3 敌人补至 10-15 种（D-05）、S6-F2 跨局记忆（D-12）、全部 Sprint 排期表（已作废）。

# 《山海镜》MVP Feature Spec & Sprint Backlog v1.1

> 基于 TDD v0.3、GDD v0.4
> **1 人 × 约 7 周（独立开发，截止 2026-09-15）**
> 一句话定位：**每个 Sprint 结束时都有一个可玩构建，Sprint 3 结束必须见到"AI 观察我并调整下一层"的完整闭环。**
>
> **v1.1 变更（美术退出 → 单人）**：美术成员退出，项目转单人开发。**下文所有 Spec 的「负责人：程序 + 美术」一律改读为「程序（系统）+ AI 美术管线（AI 生成美术/技术美术，均程序本人产出）」**，不再逐条标注。美术资产来源改为「AI 生成美术 + 原创技术美术」，第三方授权/采购资产不用（比赛要求原创+完整独立知识产权）。各 Spec 的**功能验收逻辑不变**，仅美术相关验收项从「bespoke 手工美术达标」降级为「AI 生成 + 技术美术达标、风格统一即可」。单局目标以 5 层为主选、6 层为拉伸。详见 TDD §0 / §6 / §7 与 GDD §12.6。

---

# 第一步：系统优先级拆解

遍历 TDD 中所有子系统，按"玩家能否感知到它的不存在"定优先级。

| 系统 | 优先级 | 理由 |
|---|---|---|
| 玩家移动 + twin-stick 瞄准 | **P0** | 没有这个连游戏都不是 |
| 基础攻击（判定 + 伤害 + 敌人死亡） | **P0** | 同上 |
| 玩家属性（HP + 死亡） | **P0** | 死亡 = 一局结束 |
| 俯视角摄像机 | **P0** | 游戏可见的前提 |
| 敌人个体 AI（1 种，发现-追击-攻击） | **P0** | 没有怪可打 |
| 闪避（无敌帧 + 冷却） | **P0** | 动作手感的基础，且是画像采集的"受击次数"数据源 |
| 遭遇系统（刷怪 + 波次 + 房间清空） | **P0** | 打完一波再来一波，房间有终点 |
| 武器切换（主/副手） | **P1** | "策略切换"画像维度的数据源 |
| 主动技能（2 槽） | **P1** | 丰富战斗选择，画像的"技能依赖"维度 |
| 削韧 / Hit Stop | **P1** | 打击感的基础，但不影响闭环 |
| 4 种敌人原型（Grunt/Tank/Rush/Shooter） | **P1** | AI Director 的"刷怪权重调整"需要至少 3-4 类标签怪才可见 |
| 房间模板 + 层节点图 | **P1** | 多房间 = 玩家需要选择路线 |
| 事件总线 EventBus | **P1** | 画像采集的地基，没它后面全堵住 |
| 行为记录器 BehaviorRecorder | **P1** | AI Director 的数据输入 |
| 画像分析器 ProfileAnalyzer | **P1** | 五维画像 = AI 能"理解玩家"的前提 |
| 本地导演核心 LocalDirectorCore | **P1** | 方案 A 的心脏——**没有 LLM 也能跑的 AI 调整** |
| 规则管理器 RuleManager | **P1** | AI 修改规则的执行层 |
| 关卡生成器 FloorGenerator | **P1** | 消费导演决策，造下一层 |
| 导演报告 UI（占位） | **P1** | 透明性 = 玩家知道 AI 在干什么 |
| 强化三选一（Pick 3） | **P1** | Roguelike 成长感的基础 |
| 第 2 角色（方士） | **P1** | "不同角色 → 不同画像"的演示必需 |
| 完整 6 层流程 | **P1** | 30min Demo 的骨架 |
| 烛龙 Boss（3 阶段 + 1 动态技能） | **P1** | 一局的"毕业考试" |
| LLM 集成 GameNPC + 白泽台词 | **P2** | 让 AI 会"说话"，比赛展示的核心加分项 |
| 导演报告精修 UI | **P2** | 视觉完成度 |
| 镜界时间轴 | **P2** | 演示素材，答辩用 |
| Director 开/关对照开关 | **P2** | 答辩最强武器 |
| 简版跨局记忆 | **P2** | 也是 30min 保底手段 |
| AI 视觉反馈（镜面特效） | **P2** | 玩家不读文字也知道 AI 在介入 |
| 音效 / 配乐 | **P2** | 氛围感 |
| 平衡调参（6 层难度曲线） | **P2** | 保证能打通、体验好 |
| 第 3 角色 | **P3** | MVP 后 |
| 商店 / 元素系统 | **P3** | GDD v0.2 已砍 |
| 完整 Mirror Cognitive Architecture | **P3** | Design Bible 正式版内容 |
| 多 Boss / 多主题关卡 | **P3** | MVP 后 |

---

# 第二步：Feature Spec 列表

每个 Spec 按"玩家感知的功能闭环"拆，包含输入/输出/数据结构/依赖/验收标准。**编号 = Sprint 号 + 序号。**

---

## S1-F1：Twin-Stick 角色移动与瞄准

**玩家价值**：我可以用 WASD 自由移动，角色始终面朝鼠标方向，可以边后退边开火——这是"风筝型"打法的手感基础，也是 AI 后续"移动压力"规则能生效的前提。

**功能描述**：俯视角移动 + 鼠标瞄准解耦。角色朝向不随移动方向变化，始终指向鼠标在世界平面的投影点。输入使用 Enhanced Input 系统（UE5 官方推荐，不用旧版轴绑定）。

**技术范围**：`ASHMCharacter`、`ASMPlayerController`、Enhanced Input、Input Mapping Context。

**输入**：WASD（移动）、鼠标位置（瞄准）。

**输出**：角色按 WASD 在世界移动，旋转跟随鼠标位置。

**数据结构**：无新增 DataTable。一个 `UInputAction` 资产 `IA_Move`（Vector2D）、`IA_Aim`（Vector2D）。

**依赖**：无（这是整个项目的第一个 Spec）。

**验收标准**：
- [ ] 按 W/A/S/D，角色分别朝上/左/下/右移动，速度可配置
- [ ] 角色始终面朝鼠标光标位置（不是移动方向）
- [ ] 同时按 W + 鼠标在角色身后时，角色边后退边面朝后方——即"边退边射"
- [ ] 松开所有键角色静止
- [ ] 移动不卡顿，帧率 ≥ 60fps（空场景）

**负责人**：程序

---

## S1-F2：基础近战攻击

**玩家价值**：按左键能打出近战攻击，命中敌人有伤害数字和短暂的命中停顿（打击感），敌人血条在减。

**功能描述**：近战攻击，从角色朝向发射一个扇形判定（Overlap），命中敌人扣血、显示伤害数字、触发命中停顿（仅暂停攻击者的攻击动画 0.03s）。攻速限制（防止连点）。

**技术范围**：`ASHMCharacter`、`USHMAttributeComponent`（HP）、攻击判定（扇形 Overlap）、伤害数字 Widget。

**输入**：鼠标左键。

**输出**：前方扇形区域内的敌人受到伤害，头顶弹出伤害数字。

**数据结构**：`USHMAttributeComponent` 含 `float CurrentHP, MaxHP`。武器数据先硬编码：基础伤害 20，攻速间隔 0.4s，扇形角度 90°，距离 200cm。

**依赖**：S1-F1（先能动才能打）。

**验收标准**：
- [ ] 按左键，角色播放攻击动画/蒙太奇
- [ ] 前方扇形 90°/2m 内有敌人时，敌人 HP 扣除
- [ ] 敌人头顶冒出伤害数字（UMG WidgetComponent）
- [ ] 攻击间隔不小于 0.4s（连点左键不会加速）
- [ ] 攻击命中时攻击者动画短暂停顿 ~0.03s
- [ ] 攻击没命中时无停顿、无伤害

**负责人**：程序

---

## S1-F3：第一种敌人（Grunt——近战杂兵）

**玩家价值**：房间里有敌人，敌人会发现我、走过来、打我。我被打会掉血。杀死敌人后它消失。这是"有怪可打"的最简闭环。

**功能描述**：单种敌人 `ASHMEnemy`（Grunt 原型），使用共享行为树，发现玩家 → 走到攻击范围 → 播放攻击动画/命中判定 → 循环。有 HP，HP 归零后死亡消失。

**技术范围**：`ASHMEnemy`（C++ 基类）、`UBehaviorTree`、`AI Perception`（视觉）、`UBlackboardComponent`。

**输入**：无（行为树自主运行，参数来自 DataTable 行）。

**输出**：敌人自主完成发现-追击-攻击-死亡循环。

**数据结构**：

```cpp
USTRUCT() struct FEnemyRow : public FTableRowBase
{
    FGameplayTag ArchetypeTag = FGameplayTag::RequestGameplayTag("Enemy.Grunt");
    float HP = 80; float Speed = 300; float ContactDamage = 15;
    float AttackRange = 150; float AttackCooldown = 1.2f;
};
```

**依赖**：S1-F1（玩家存在才能感知）、S1-F2（敌人 HP 扣减逻辑复用同一个属性组件）。

**验收标准**：
- [ ] 游戏开始时，房间内生成了 ≥ 1 个 Grunt
- [ ] Grunt 在 20m 外不理会玩家（Idle/Patrol 状态）
- [ ] 玩家进入 ~8m 范围后 Grunt 开始追击
- [ ] Grunt 走到 1.5m 内对玩家发动攻击，玩家扣血 ≥ 1 次
- [ ] 玩家攻击 Grunt，Grunt HP 归零后从场景中移除
- [ ] 2-3 个 Grunt 同时在场时不卡顿

**负责人**：程序（C++ 基类 + BT）+ 美术（Grunt 模型/Material/死亡特效）

---

## S1-F4：俯视角摄像机 + 玩家 HP 与死亡

**玩家价值**：整个场景可见，游戏不是黑屏。我 HP 归零后角色死亡，游戏告诉我"你死了"——游戏有失败条件。

**功能描述**：固定俯视角（俯仰约 -70°，高度 ~1500cm），摄像机轻微跟随玩家。HP 组件：HP 从最大值开始，受击扣除，≤ 0 触发死亡。

**技术范围**：`ASMPlayerController`（已有 `SetupTopDownCamera` 骨架）、`USHMAttributeComponent`、`ASMGameMode`（死亡后处理）。

**输入**：摄像机：玩家位置。HP：受击事件。

**输出**：摄像机随玩家平滑移动（延迟 ~0.1s 跟随，不抖）。HP ≤ 0 时禁用输入 + 显示"试炼结束"文字 + 3s 后重启或回到主菜单。

**数据结构**：无新增。`USHMAttributeComponent` 复用 S1-F2 的，加一个 `OnDeath` 委托。

**依赖**：S1-F1（移动）、S1-F3（敌人能攻击玩家）。

**验收标准**：
- [ ] 摄像机以俯视角 ~70° 俯视，玩家始终在画面中央区域
- [ ] 摄像机平滑跟随，不抖动
- [ ] HUD 左上角显示 HP 条（红底绿条，当前值/最大值）
- [ ] 玩家 HP 归零后：输入全部禁用、屏幕中央显示"试炼结束"、3s 后自动返回
- [ ] 玩家 HP 不会低于 0 或显示负数

**负责人**：程序

---

## S1-F5：Sprint 1 集成——一个可打怪的测试房间

**玩家价值**：这是一个可玩构建。我进去能动、能打、有怪能杀、死了会结束。不用命令行、不用编辑器 hack。

**功能描述**：在 `SMGameMode` 的测试关卡（`L_TestArena`）中，完整跑通：玩家出生 → 移动、瞄准、攻击 → 3-5 个 Grunt 从四周刷出 → 杀光所有敌人 → 屏幕显示"房间清空" → 一个出口传送门出现。

**技术范围**：`ASMGameMode`、`L_TestArena` 关卡、占位遭遇逻辑（直接 spawn 到关卡里，还不需要正式的 EncounterManager）。

**输入**：玩家进入关卡。

**输出**：完整的"进入房间 → 战斗 → 清空 → 告诉玩家"的循环一次。

**数据结构**：`DT_Enemy` 里 1 行（Grunt）。`L_TestArena` 关卡里一个 NavMesh Bounds Volume。

**依赖**：S1-F1 ~ S1-F4。

**验收标准**：
- [ ] 双击 .uproject → 进入游戏（Standalone）→ 自动加载 L_TestArena
- [ ] 玩家可自由移动/攻击
- [ ] 3-5 个 Grunt 分布在地图各处，逐个被玩家吸引/击杀
- [ ] 全部 Grunt 死亡后，HUD 显示"房间清空"，房间中央出现一个传送门（占位即可）
- [ ] 玩家站上传送门 1s 后，屏幕黑屏 → 回到主菜单或重新开始
- [ ] 中途死亡也回到主菜单

**负责人**：程序（关卡逻辑、游戏流程）+ 美术（L_TestArena 的视觉：地面、墙壁、灯光、NavMesh）

---

**====== Sprint 1 完成标志 ======**
**可玩构建：一个房间、一个角色、一种敌人、能打能死。**

---

## S2-F1：闪避系统（无敌帧 + 冷却）

**玩家价值**：按右键/Shift 可以翻滚闪避。闪避期间一小段无敌（无敌帧），闪避后有冷却。这是我规避伤害的核心手段。AI 后续采集我的"闪避成功率"作为战斗效率维度的输入。

**功能描述**：按下闪避键 → 角色朝当前移动方向（若无移动输入则朝鼠标方向）快速位移 ~4m → 闪避期间的前 0.25s 内不受伤害（无敌帧）→ 冷却 0.8s。闪避可取消当前攻击动画。

**技术范围**：`ASHMCharacter` + `USHMAbilityComponent`（闪避作为内置能力，不算在 2 槽技能里）。

**输入**：右键 / Left Shift。

**输出**：角色朝指定方向快速移动 4m，0.25s 无敌，之后 0.8s 内无法再次闪避。

**数据结构**：
```cpp
// USHMAbilityComponent 的闪避配置
float DodgeDistance = 400.f;
float DodgeDuration = 0.4f;
float DodgeIFrameDuration = 0.25f;
float DodgeCooldown = 0.8f;
```

**依赖**：S1-F1（移动）。

**验收标准**：
- [ ] 按闪避键，角色朝移动方向（或鼠标方向）快速位移约 4m
- [ ] 闪避的前 0.25s 内，敌人攻击命中不扣血——能看到"Miss"或特效而非伤害数字
- [ ] 闪避期间攻击动画被取消（你可以在攻击前摇中闪避）
- [ ] 闪避结束后 0.8s 内再次按闪避键无效，HUD 闪避图标显示冷却转圈
- [ ] 原地不动按闪避 = 朝鼠标方向闪

**负责人**：程序

---

## S2-F2：武器切换（主/副手）+ 猎弓远程攻击

**玩家价值**：我可以用近战武器砍，也可以切远程弓射。不同的武器有不同的攻击方式，我开始有"打法选择"了。

**功能描述**：空格/滚轮切武器。主手=重剑（近战扇形），副手=猎弓（远程投射物）。弓的攻击方式：朝鼠标方向发射一支箭矢（Projectile），飞行命中敌人后扣血消失，未命中飞行 ~30m 后消失。武器切换没有冷却。

**技术范围**：`USHMWeaponComponent`（C++ 组件，持有主副武器数据，管理切换）、`AProjectile`（投射物基类，`SMArrow` 是其子类）。

**输入**：空格/滚轮（切武器）。左键攻击会读取当前活跃武器的攻击模式。

**输出**：按空格后角色武器切换，HUD 武器图标更新。左键的攻效取决于当前武器（剑=扇形，弓=投射物）。

**数据结构**：
```cpp
// USHMWeaponComponent
USTRUCT() struct FWeaponSlot { FGameplayTag Tag; EAttackPattern Pattern; float Damage; float Speed; /*...*/ };
FWeaponSlot MainWeapon; // 剑
FWeaponSlot SubWeapon;  // 弓
bool bMainWeaponActive = true;
```

**依赖**：S1-F2（近战攻击逻辑）、S1-F1（鼠标瞄准方向给弓用）。

**验收标准**：
- [ ] 默认拿剑，左键是近战扇形攻击
- [ ] 按空格后切到弓，HUD 武器图标变化，左键变为发射箭矢（朝鼠标方向）
- [ ] 箭矢飞行命中敌人后扣血敌人并销毁箭矢
- [ ] 箭矢未命中时飞行约 30m 后自动销毁
- [ ] 再按空格切回剑，攻击方式变回近战
- [ ] 武器切换没有冷却，战斗中反复切不卡顿
- [ ] 弓和剑的基础伤害不同（能明显感到弓单发伤害更低但安全）

**负责人**：程序

---

## S2-F3：4 种敌人原型 + 共享行为树

**玩家价值**：房间里不再全是同一种怪。盾兵正面无敌逼我走位，突进怪冲我脸，射手远程射我——我需要根据敌人组合改变战斗方式。每一种敌人都有明确的外观和行为，我第一眼就知道它会干什么。

**功能描述**：4 种原型全部实装，每种至少 1 个具体异兽填入 `DT_Enemy`：

| 原型 | DT 标签 | 代表异兽 | 核心行为 |
|---|---|---|---|
| Grunt | `Enemy.Grunt` | 魑魅 | 走近平A（S1-F3 已有，直接继承） |
| Tank | `Enemy.Tank` | 玄龟 | 举盾正面无敌，缓慢推进（BT 新增 Shield 节点） |
| Rush | `Enemy.Rush` | 穷奇 | 高速直线冲锋 + 扑击（BT 新增 Charge 节点） |
| Shooter | `Enemy.Shooter` | 鸣蛇 | 保持距离，远程射击（BT 新增 RangedAttack 节点） |

所有敌人**共享一棵行为树**，差异来自 Blackboard 参数 + DataTable 行 + 各自的 Attack 子树（蓝图实现）。

**技术范围**：`ASHMEnemy`、共享 BT、`DT_Enemy`、各原型蓝图的攻击子树。

**输入**：BT 从 Blackboard 读取参数（速度、攻击距离、攻击冷却等），Blackboard 数据从 DT 行初始化。

**输出**：4 种行为截然不同的敌人，可被 AI Director 通过原型标签独立调权重。

**数据结构**：`DT_Enemy` 扩充至 4 行（S1 只有 Grunt 1 行）。

```cpp
// 新增字段
FGameplayTagContainer CounterTags;  // Tank 克制 Build.Ranged, Rush 克制 Build.Ranged
int32 ThreatValue;                   // Grunt=5, Tank=20, Rush=15, Shooter=18
```

**依赖**：S1-F3（Grunt 已通的 BT 框架）。

**验收标准**（每种原型一项）：
- [ ] Tank（玄龟）：正面有护盾特效，玩家的远程箭矢打正面不扣血（或只扣 10%），走到近战范围盾不防近战
- [ ] Rush（穷奇）：在 ~10m 外锁定玩家，低头/吼叫 ~0.5s 前摇后高速冲过来，撞到玩家扣血 + 击退
- [ ] Shooter（鸣蛇）：始终保持 ~8m 距离，间断朝玩家发射投射物，被近身会后退
- [ ] 每种原型的外观/轮廓明显不同——玩家 0.3s 内能分辨"这是个什么怪"
- [ ] 4 种怪同时在一个房间内不共享同一棵 BT 实例（每个敌人独立决策）

**负责人**：程序（BT 框架 + 各 Attack 节点的 C++ 基类）+ 美术（4 种怪的模型/Material/动画/特效）

---

## S2-F4：遭遇系统（波次刷怪 + Threat Budget + 房间清空判定）

**玩家价值**：不是一进房间所有怪堆在脸上——敌人按波次出现，一波接一波，有节奏感。杀光全部波次后房间清空，我知道接下来该去下一个房间了。

**功能描述**：`UEncounterManager`（`UActorComponent` 挂在 `ASMGameMode` 上）。房间模板的`SpawnPoint`标记敌人出生点。遭遇配置定义 N 波，每波的敌人类型比例 + 总ThreatBudget。管理器按波次顺序刷怪，监控剩余敌人数，全部死亡 → 下一波 → 最后一波清空 → 广播 `OnRoomCleared`。

**技术范围**：`UEncounterManager`、`ASpawnPoint`（编辑器可摆的 Actor）、`FRoomEncounterConfig` 结构。

**输入**：进入房间触发 `BeginEncounter`。

**输出**：波次刷怪 → 全部清空 → 房间清空事件。

**数据结构**：
```cpp
USTRUCT() struct FWaveConfig { TMap<FGameplayTag, float> ArchetypeRatios; int32 Budget; };
USTRUCT() struct FRoomEncounterConfig { TArray<FWaveConfig> Waves; float WaveDelay; };
```

**依赖**：S2-F3（所有 4 种原型都可用）、S1-F5（L_TestArena 已有占位出口）。

**验收标准**：
- [ ] 进入房间 → Wave1 按 Budget+比例刷出敌人（如 Budget 40 = Tank 20 + Rush 15 + Grunt 5 = 2 Tank + 1 Rush + 1 Grunt）
- [ ] Wave1 全部死亡后 ~2s 间隔，Wave2 刷出
- [ ] 全部 Wave 清空后 HUD 显示"房间清空"，出口传送门出现
- [ ] 敌人不会刷在玩家视野正中心（SpawnPoint 距玩家 ≥ 10m）
- [ ] 如果某波刷出的怪卡在墙里/无法寻路，编辑器里能看到红框警告

**负责人**：程序

---

## S2-F5：房间模板 + 多房间流程（2 个房间串起来）

**玩家价值**：清完一个房间，走进传送门，进入下一个房间。游戏不再是单房间 Demo——"探索感"开始出现。两个房间可以用不同的模板。

**功能描述**：美术制作 2 个房间关卡模板（`L_Room_Open` 开阔型、`L_Room_Narrow` 狭窄多掩体型），用 UE Level Streaming 加载。`UFloorManager` 管理当前层的房间顺序。每次进入新房间时，`EncounterManager` 读取该房间模板的遭遇配置并启动。

**技术范围**：`UFloorManager`、Level Streaming、2 个房间关卡、`ASMRoomPortal`（传送门 Actor）。

**输入**：`L_Room_Open` + `L_Room_Narrow` 两个关卡文件 + 各自的 SpawnPoint + NavMesh。

**输出**：玩家可以在 2 个房间之间顺序推进。

**数据结构**：
```cpp
USTRUCT() struct FRoomTemplateRow : public FTableRowBase
{
    TSoftObjectPtr<UWorld> Level;            // 房间子关卡
    TArray<FGameplayTag> ArenaTags;          // 开阔/狭窄/多掩体
    FRoomEncounterConfig DefaultEncounter;   // 默认遭遇（会被 Director 覆盖）
};
```

**依赖**：S2-F4（遭遇系统）、S1-F5（出口传送门）。

**验收标准**：
- [ ] 游戏开始后加载第一个房间（L_Room_Open），房间内正确刷怪
- [ ] 清空后出口传送门出现，站上 1s → 流式加载第二个房间（L_Room_Narrow），卸载第一个
- [ ] 第二个房间的地理和第一个明显不同（一个开阔一个窄）
- [ ] 第二个房间的遭遇配置也不同（比如更多 Tank 利用狭窄地形推进）
- [ ] 流式加载过程不黑屏超过 2s，不掉帧

**负责人**：程序（FloorManager + Level Streaming 代码）+ 美术（做 2 个房间关卡，含地形/装饰/NavMesh/SpawnPoint）

---

**====== Sprint 2 完成标志 ======**
可玩构建：一个角色（力士）、4 种敌人、闪避、弓+剑切换、波次遭遇、两个房间串起来。

---

## S3-F1：事件总线 + 行为记录器（画像数据地基）

**玩家价值**：玩家感知不到它——但它是整个 AI Director 的地下管道。之后每层结束 AI 能分析我的战斗风格，全靠这套埋点。

**功能描述**：`UGameInstanceSubsystem` 实现 `UEventBus`，全局广播玩法事件。`UBehaviorRecorder`（另一个 Subsystem）订阅事件，把每层的战斗行为累积到 `FFloorBehaviorSnapshot`。层结束时定稿快照，供画像分析器消费。

**技术范围**：`UEventBus`、`UBehaviorRecorder`。

**输入**：各玩法系统通过 EventBus 广播的事件（`OnAttack` `OnHitTaken` `OnDodge` `OnSkillCast` `OnKill` `OnWeaponSwitch` `OnItemUse` `OnLowHp` `OnRoomFinished` `OnFloorFinished`）。

**输出**：层结束时一个定稿的 `FFloorBehaviorSnapshot`。

**数据结构**：完全对齐 TDD §3.1 的 `FFloorBehaviorSnapshot`，含 5 维度 14 项数据。

```cpp
USTRUCT() struct FFloorBehaviorSnapshot
{
    int32 FloorIndex;
    // 维度1 Build集中度
    TMap<FGameplayTag, int32> WeaponAttackCounts;
    TMap<FGameplayTag, int32> SkillCastCounts;
    TMap<FGameplayTag, float> DamageByTag;
    TMap<FGameplayTag, int32> KillsByTag;
    // 维度2 战斗效率
    float RoomClearTimeAvg; int32 HitsTaken; float TotalDamageTaken; float DodgeSuccessRate;
    // 维度3 资源盈余
    int32 HealItemsRemaining; int32 AmmoRemaining;
    // 维度4 策略切换
    int32 WeaponSwitchCount; TSet<FGameplayTag> UniqueSkillsUsed; float BuildDiffFromLastFloor;
    // 维度5 生存压力
    int32 LowHpEvents; int32 DeathOrNearDeath;
};
```

**依赖**：S2-F2（武器切换事件）、S2-F1（闪避事件）、S1-F2（攻击事件）、S2-F4（房间结束事件）。

**验收标准**：
- [ ] 打一个房间后，在控制台输入 `ShowDebug Behavior`，能打印出 `WeaponAttackCounts`（含弓和剑的次数）、`HitsTaken`、`DodgeSuccessRate`
- [ ] 弓用 90% 攻击次数时，打印结果与主观感觉一致
- [ ] `OnFloorFinished` 触发后，`GetSnapshot()` 返回非空数据
- [ ] EventBus 不丢事件——打 50 个敌人验证计数器与手动数的一致

**负责人**：程序

---

## S3-F2：画像分析器（五维评分）

**玩家价值**：仍然感知不到——但它是 AI "理解"我的数学引擎。打完一层后，AI 知道我"远程 90%、受击少、不换武器"，准备下一层针对我。

**功能描述**：`UProfileAnalyzer`（纯 C++ 无状态的静态函数类），输入 `FFloorBehaviorSnapshot` + 历史快照（最近 3 层），输出 `FPlayerProfile`（五维 0-100）+ 置信度 + 主导原型标签。

**技术范围**：`UProfileAnalyzer`。

**输入**：`FFloorBehaviorSnapshot`（本层）+ `TArray<FFloorBehaviorSnapshot>`（前 3 层）。

**输出**：`FPlayerProfile`。

**数据结构**：对齐 TDD §3.2。

```cpp
USTRUCT() struct FPlayerProfile
{
    float BuildConcentration;     // 主武器攻击次数/总攻击次数 ×100
    float CombatEfficiency;       // 清怪速度、受击、闪避的综合加权
    float ResourceSurplus;        // 剩余药/弹药越多越高
    float StrategySwitch;         // 切武器频率 + 不同技能数 + 层间 Build 差异
    float SurvivalPressure;       // 低血/濒死越多越高
    FGameplayTag DominantArchetype; // Ranger/Vanguard/Berserker/Survivor
    TArray<FGameplayTag> PrimaryBuildTags;
    float Confidence;             // 连续 N 层同原型 → 置信度高
};
```

**算法示例**（BuildConcentration）:
```text
总攻击次数 = Σ WeaponAttackCounts.Values
最大武器 = Max(WeaponAttackCounts.Values)
BuildConcentration = (最大武器 / 总攻击次数) × 100
若 BuildConcentration > 85 且 Confidence > 0.6 → DominantArchetype = 该武器标签 → Archetype Map
```

**依赖**：S3-F1（需要先有快照数据才能打分）。

**验收标准**：
- [ ] 只拿剑打一层 → BuildConcentration ≥ 90, DominantArchetype = Vanguard, Confidence ≥ 0.7
- [ ] 剑和弓各一半 → BuildConcentration ≈ 50-60, Confidence = 0.5
- [ ] 连续 3 层远程 → Confidence ≥ 0.9
- [ ] 全程不受击 → CombatEfficiency ≥ 85, 受 10 次以上 → ≤ 50
- [ ] 算法纯 C++ 可单元测试——写 3 个 UAT 测试通过

**负责人**：程序

---

## S3-F3：本地导演核心（方案 A）——**这是全项目最大里程碑**

**玩家价值**：**我第一次感觉到"AI 在观察我"。** 打完第一层后弹出一张"导演报告"——白泽告诉我它看到了什么，下一层敌人和规则变了。第二层进去，真的感觉到了不同：因为第一层用弓太多，第二层多了盾兵、弹药少了。我在对抗的不是随机刷怪，而是一个会学我的 AI。

**功能描述**：本地导演核心 = 代码实现"画像 → 调整决策"的完整逻辑，**不依赖 LLM**。流程：

1. 读取 `FPlayerProfile`
2. 在规则池/敌人权重表中，按画像维度加权选择调整项
3. 通过 Budget / Conflict / Fairness 三道护栏过滤
4. 产出 `FDirectorDecision`

**技术范围**：`ULocalDirectorCore`（C++）、`DT_Rule`、`DT_DirectorFallback`、`URuleManager`（应用全局 Effect）。

**输入**：`FPlayerProfile` + 当前层数 + 挑战预算 `ChallengeBudget` + `DT_Rule` 规则池。

**输出**：`FDirectorDecision`。

**数据结构**：
```cpp
USTRUCT() struct FDirectorDecision
{
    int32 ChallengeLevel;                          // 0 Recovery ~ 4 Evolution
    TMap<FGameplayTag, float> EnemyWeights;        // Enemy.Tank: 0.3, Enemy.Rush: 0.2
    TArray<FRuleModifier> RuleModifiers;            // 选中的规则+等级
    TMap<FGameplayTag, float> ResourceAdjust;      // Resource.Heal: -0.15
    FGameplayTag ArenaPreference;                  // Arena.Open / Arena.Cover
    FString NarrationLine;                         // 占位台词（本地模板）
    FString Reason;                                // 内部调试用
};

USTRUCT() struct FRuleModifier
{
    FGameplayTag RuleTag;  // Rule.Ammo / Rule.Heal / Rule.Cooldown / Rule.Crit ...
    FString Level;          // light / medium / heavy（MVP 不用 heavy）
    int32 Cost;
    FString Reason;
};
```

**决策算法（核心）**：
```text
Step 1: 根据五维画像计算 ChallengeLevel
  SurvivalPressure > 70 → Level 0 (Recovery)
  BuildConcentration > 80 AND StrategySwitch < 30 → Level 2-3 (Counter/Pressure)
  默认 → Level 1-2

Step 2: 根据画像维度选择敌对侧
  BuildConcentration 高 → 提高克制 PrimaryBuildTags 的敌人权重（Tank/Rush 反远程）
  CombatEfficiency 高 → 增加精英权重、提高敌人攻速
  ResourceSurplus 高 → 减少掉落
  StrategySwitch 低 → 出中度规则，逼换打法

Step 3: 三道护栏
  Budget Check: Σ Cost ≤ ChallengeBudget[Floor]
  Conflict Check: 禁止 Rule.Ammo + Rule.RangedDamage 同时出现（无解）
  Fairness Check: 同一 Rule 不能连续 3 层出现; SurvivalPressure > 70 时不能上中度以上规则
```

**依赖**：S3-F2（画像数据）、S2-F4（遭遇能接受权重调整）。

**验收标准（这是最重要的验收）**：
- [ ] 第一层用弓打 80%+ 攻击 → 进入第二层前弹导演报告："检测到你偏好远程攻击" → 第二层 **Tank 权重 ≥ 50%**，普通敌人数明显上升盾兵比例
- [ ] 第一层全程近战剑 → 第二层 **Shooter 权重提升**，远程怪增多
- [ ] 第一层频繁挨打到 HP < 20% 3 次 → 第二层画像是 SurvivalPressure 高 → **ChallengeLevel = 0 Recovery**，只出 Grunt，规则不作负面调整
- [ ] Budget 检查生效：Floor 2 预算 35，不允许选 Cost 超过预算的组合
- [ ] Conflict 检查生效：Rule.Ammo 和 Rule.RangedDamage 不会同时出现
- [ ] 导演报告在切换层时弹出，必须手动点击"进入下一层"才能继续
- [ ] **核心验收**：同一个玩家用弓打和用剑打，第二层配置明显不同且和玩家打法有因果关系

**负责人**：程序

---

## S3-F4：规则管理器 + 导演报告 UI（占位）

**玩家价值**：导演报告弹出时，我能看到 AI 做了什么更改。"弹药掉落 -25%"显示成一张规则卡，进游戏后我捡弹药确实变少了。透明性成立。

**功能描述**：`URuleManager` 消费 `FDirectorDecision` 中的 `RuleModifiers`，将对全局施加 `USHMEffectComponent` 类型的临时效果（带时长"持续到本层结束"）。导演报告 UI 用 UMG 做简单的占位版：白泽头像 + 观察数据 + 本层调整项 + "进入下一层"按钮。

**技术范围**：`URuleManager`、`USHMEffectComponent`、`WBP_DirectorReport`（UMG Widget）。

**输入**：`FDirectorDecision.RuleModifiers`。

**输出**：全局规则 Effect 生效（弹药掉落率 × 倍率 = 玩家实际捡到的弹药数减少），导演报告 UI 显示调整项。

**数据结构**：
```cpp
// DT_Rule: 规则池（策划填的表，代码只读不写数值）
USTRUCT() struct FRuleRow : public FTableRowBase
{
    FGameplayTag RuleTag;
    FString Level;        // light / medium / heavy
    int32 Cost;
    FString DisplayName;  // 弹药不足
    FString Description;  // 弹药掉落率 -25%
    float ModifierValue;  // 应用的具体倍率（light=-0.3 medium=-0.5）
};
```

**依赖**：S3-F3（导演决策产出规则列表）。

**验收标准**：
- [ ] 导演报告中，"弹药掉落 -25%"以规则卡的形式展示
- [ ] 进入第二层后，开宝箱/打怪掉的弹药数量确实约为正常值的 75%
- [ ] "回复效果 -30%"规则下，喝药回血明显变少
- [ ] 规则只持续本层——进入第三层后上一层的规则自动失效
- [ ] 导演报告必须点击按钮才能关闭，不可跳过（确保玩家一定看到）

**负责人**：程序（RuleManager + UI 骨架）+ 美术（WBP_DirectorReport 排版/美术）

---

## S3-F5：层过渡 + 2 层完整流程（Sprint 3 里程碑集成）

**玩家价值**：**这是一个可玩的 AI Director Demo。** 完整打出 2 层：第一层打完之后看导演报告，第二层的体验因为我的打法而不同。AI Native 闭环首次可见。

**功能描述**：第一层（2 个房间） → 层结束 → 导演报告弹出 → 玩家确认 → 第二层（2 个房间，已被导演修改）→ 层结束 → 第二个导演报告 → 游戏结束/显示"Demo 完成"。

**技术范围**：`UFloorManager` 扩展到支持多层、`ASMGameMode` 管理 Run 流程。

**输入**：第一层 Room1+Room2 已清空。

**输出**：完整的 2 层 Demo 流程（含导演报告 + 规则生效 + 遭遇差异化）。

**依赖**：S3-F1 ~ S3-F4。

**验收标准**：
- [ ] 完整打出 2 层（每层 2 个房间），中间经过导演报告，总计约 8-10 分钟
- [ ] 第一层和第二层的敌人组合**因玩家打法而明显不同**（这是核心验收——如果两层的刷怪配置看起来一样，说明 Director 没生效）
- [ ] 规则在第一层→第二层过渡时生效，在第二层→结束过渡时被卸载
- [ ] 从第一层第一个房间到打完第二层最后一个房间，全程不崩溃、不卡加载

**负责人**：程序

---

**====== Sprint 3 完成标志 ======**
**"AI 观察我并调整下一层"的完整闭环可玩。没有 LLM，但 AI Native 已成立。这是项目的安全下限。**

---

## S4-F1：强化三选一（Pick 3）+ 被动叠加

**玩家价值**：每层清空后，在导演报告之后，弹出 3 张卡片让我选 1 个强化——暴击率 +10%、近战伤害 +15%、还是击杀回血？我选的强化永久保留到本局结束（Roguelike 经典成长感）。AI 也会偷偷调整选项，诱导我换打法。

**功能描述**：清完一层 → 导演报告 → Pick 3 界面。3 个强化从 `DT_Passive` 随机抽，但 `ULocalDirectorCore` 可对池子施加权重偏移（如玩家 90% 远程时，近战强化出现概率 +30%）。玩家选中的被动加入 `USHMBuildComponent`（叠加），通过属性组件的 Bonus 生效。

**技术范围**：`USHMBuildComponent`、`WBP_Pick3` UMG、`DT_Passive`。

**输入**：`DT_Passive`（~15 行）+ Director 的诱导权重。

**输出**：玩家获得 1 个永久被动，属性/HUD 即时更新。

**数据结构**：
```cpp
USTRUCT() struct FPassiveRow : public FTableRowBase
{
    FGameplayTag Tag;           // Passive.CritUp / Passive.MeleeUp / ...
    FGameplayTagContainer BuildTags; // Build.Melee / Build.Ranged / Build.Crit / ...
    FString DisplayName;
    FString Description;
    float Value;                 // 暴击率+0.10
    FName TargetAttribute;       // 作用在哪个属性上（CritChance / MeleeDamage / ...）
};
```

**依赖**：S3-F5（层过渡流程已通，在导演报告之后插入即可）。

**验收标准**：
- [ ] 每层清空后、进下一层前，弹出 3 张强化卡片
- [ ] 选择后该强化永久生效（如选"近战伤害 +15%"，之后近战攻击的伤害数字确实升高）
- [ ] HUD 上能查看已持有的强化列表
- [ ] 远程流派玩家看到近战强化出现概率明显高于远程强化（诱导生效）
- [ ] 退出游戏重进不保留（本局内有效，符合 Roguelike 逻辑）
- [ ] 不在 3 张里选的卡不是"惩罚"（没有"移速 -10%"这种情况）

**负责人**：程序

---

## S4-F2：第 2 角色——方士（远程法师）

**玩家价值**：除了力士（肌肉近战），现在可以选方士（远程法术）。两个角色的起始武器、技能、打法完全不同。AI 给方士和给力士的针对方案也应该不同。

**功能描述**：方士 `BP_Roles_Fangshi : ASHMCharacter`。起始主武器 = 卦镜（法术穿透 AOE），副武器 = 灵幡。起始 HP 低（~70% 力士）、移速略快。主动技能 = 灵符（远程标记增伤）。

**技术范围**：BP 子类、`DT_Weapon` 扩充 2 行（卦镜 + 灵幡）、角色选择 UI。

**输入**：主菜单选角色。

**输出**：方士完整可用，战斗风格与力士明显不同。

**验收标准**：
- [ ] 主菜单可二选一（力士/方士）
- [ ] 方士血量明显低于力士（~70%）、移速略快
- [ ] 卦镜攻击 = 前方直线穿透，可同时打到一排敌人
- [ ] 灵符 = 远程标记 + 增伤 debuff（被标记的敌人受到伤害 +20%，持续 3s）
- [ ] 用方士远程风筝 AI Director → 画像显示 Ranger → 下一层 Tank 增多 ✓

**负责人**：程序（基类支持角色差异化）+ 美术（方士模型/Material/动画）

---

## S4-F3：6 层完整流程 + 烛龙 Boss

**玩家价值**：**可以完整打一局了。** 6 层、约 30 分钟、最后一层打烛龙。打过 Boss 有胜利结算，死了有失败总结。这是一局完整的 Roguelike。

**功能描述**：6 层流程（含 F3 精英遭遇点、F6 Boss 烛龙），每层清空 → 导演报告 → Pick 3 → 下一层。烛龙 = 独立 Boss 蓝图，3 阶段（昼/交替/永夜）+ 1 个动态技能（读取画像选择：远程→Mirror Dash 突进、近战→Fire Ring 近身 AOE、高爆发→沉默）。Boss 战中不再实时调用 AI，技能在战前选定。

**技术范围**：6 层 `FloorManager`、`ABoss_Zhulong`（独立蓝图 + BT/状态机）、Boss 技能系统、胜利/失败结算 UI。

**输入**：6 层房间数据 + F6 为 Boss 层标记。

**输出**：可完整打完一局 30 分钟。

**数据结构**：
```cpp
USTRUCT() struct FBossSkill
{
    FGameplayTag SkillTag;
    FGameplayTag CounterTag;     // 克制哪种 Archetype
    FString DisplayName;
    TSoftObjectPtr<UAnimMontage> Anim;
};
```

**依赖**：S4-F1（Pick 3 培养 Build）、S4-F2（两个角色都能打 Boss）、S3-F5（2 层流程已验证通过，扩展到 6 层）。

**验收标准**：
- [ ] 从主菜单选角色 → 完整打完 6 层（含 Boss）→ 胜利结算，约 25-35 分钟
- [ ] F3 出现精英遭遇（一个带词条的 Elite + 普通怪）
- [ ] 烛龙 3 阶段完整运行：昼→交替→夜，阶段间有转场动画/特效
- [ ] 烛龙 1 个动态技能会根据角色画像正确选择（方士→Mirror Dash，力士→Fire Ring）
- [ ] 死亡后在结算界面显示"失败原因"文字（取自导演分析，至少是模板文字）
- [ ] 胜利后显示"镜界评价"（占位版）
- [ ] 全程不掉帧，Boss 战帧率 ≥ 50fps

**负责人**：程序（Boss 逻辑/状态机、完整 6 层流程）+ 美术（烛龙模型/Material/特效/3 阶段转场、胜利/失败 UI、精英怪外观）

---

**====== Sprint 4 完成标志 ======**
**完整的 30 分钟 Roguelike Demo，含 2 角色 + Build 成长 + Boss + 本地 AI Director。LLM 还没接，但玩法完整体验已全。这是项目的"内容下限"。**

---

## S5-F1：AIService——GameNPC 集

成 + JSON 校验 + 三级兜底

**玩家价值**：除了 AI 会调整关卡，现在白泽还会用自然语言跟我说话——每局的台词不同，不像是模板。实际上 LLM 生成的台词背后是 LLM 在帮我选策略。

**功能描述**：`UAIService` 封装 `IAIProvider` 接口。`FGameNPCProvider` 实现真实的 GameNPC HTTP 调用，`FDebugProvider` 用 OpenAI 兼容端点做开发期调试。异步请求 → 收 JSON → `UJsonValidator` 校验（Schema 不合法 → 重试 1 次 → 仍失败进入兜底）→ 返还 `FDirectorDecision`。LLM 只决策"策略+台词"部分，不接触具体数值；数值仍然由 `URuleManager` 从 `DT_Rule` 查表映射。**三级兜底：缓存上层结果 → 本地规则表 → 固定流程（方案 A）。**

**技术范围**：`UAIService`、`FGameNPCProvider`、`UJsonValidator`、`ULocalDirectorCore`（兜底时调用）。

**输入**：`FPlayerProfile` + 当前关卡信息（对齐 TDD §3.4 的 JSON 输入）。

**输出**：`FDirectorDecision`（来自 LLM + 验证，或来自兜底）。

**数据结构**：完全对齐 TDD §3.4，输入输出均为结构化 JSON。

**依赖**：S3-F3（本地 Director 已跑通——LLM 是叠在上面的增强层）。

**验收标准**：
- [ ] GameNPC API 能正常连通，单次往返 ≤ 5s
- [ ] LLM 返回的 JSON 通过 Schema 校验后成功应用到下一层
- [ ] 故意让 LLM 返回非法 JSON（漏字段），系统自动检测并 **1 次重试** 后走兜底
- [ ] API 超时 3s 后自动走兜底（不等死）
- [ ] 连续 3 次失败后切换到完全本地 Director 模式，游戏继续可玩
- [ ] 不开启 LLM 时（调试开关），游戏用方案 A 正常运行
- [ ] 白泽台词由 LLM 生成，与本地模板句明显不同（语义更丰富、更具体）
- [ ] **安全验收**：LLM 返回的数值（如果有）会被忽略，数值永远从 DT_Rule 查表

**负责人**：程序

---

## S5-F2：白泽台词 + 导演报告精修

**玩家价值**：导演报告不再是一堆数字。白泽用山海经式的半文言语调跟我说话——"你的弓使得纯熟，但镜界不会永远纵容同一把武器。"每局的台词不同，我感受到白泽有"人格"。

**功能描述**：LLM 生成的白泽台词填入导演报告 + 规则卡。导演报告 UI 精修为接近 TDD §3 的最终版面：白泽头像 + 观察数据 + "本层调整"规则卡（配镜面特效）+ 白泽台词 + "进入下一层"。战斗中白泽偶尔有简短提示（≤ 2s,不遮挡战斗）：Boss 获得 AI 技能时→"看来它已找到你的弱点"。

**技术范围**：`WBP_DirectorReport`（精修）、`WBP_RuleCard`、白泽战斗提示 Widget。

**输入**：LLM 生成的 Narration 字符串、DirectorDecision。

**输出**：有设计感的导演报告 + 白泽人格化台词。

**依赖**：S5-F1（LLM 产出台词内容）、S3-F4（已有占位 UI 骨架）。

**验收标准**：
- [ ] 导演报告的台词 2 次 Run（同一打法）内容不同
- [ ] 台词风格始终如一：山海经/文言风格、冷静观察、不失温和
- [ ] 报告布局清晰可读，打开 ≤ 10s 就能理解 AI 为什么调整
- [ ] 战斗中 Boss 动态技能触发时 ≤ 2s 出现白泽提示文字，不遮挡主要战斗区域
- [ ] 每张规则卡有对应的镜面裂纹/符文特效区分类别

**负责人**：程序（集成 LLM 台词落地）+ 美术（导演报告终版 UI + 规则卡视觉 + 白泽头像 + 镜面特效）

---

## S5-F3：补充敌人至 10-15 种 + AI 视觉反馈

**玩家价值**：敌人种类更丰富——同是盾兵，有玄龟也有石敢当（外观不同、数值微调），重复游玩不腻。AI 调整规则/刷怪时，屏幕边缘有镜面裂纹光效——我不看导演报告也知道"世界变了"。

**功能描述**：`DT_Enemy` 从 Sprint 2 的 4 行扩充至 10-15 行（每个原型 2-3 个变体），变体之间共享原型 BT + 攻击逻辑，差异在数值/外观/特效。AI 视觉反馈 = 统一的镜面裂纹/蓝白流光/符文动画，在规则生效、Boss 动态技能触发、Arena Rule 改变时播放。

**技术范围**：`DT_Enemy` 行填充、各原型变体蓝图（继承基类 BP）、`AMirrorVisualFX`（播放镜面特效的管理器）。

**输入**：美术资源（模型/Material/特效）+ 策划数值（每行的 HP/伤害/速度/ThreatValue）。

**输出**：10-15 种可刷的异兽 + 统一的 AI 视觉语言。

**依赖**：S2-F3（4 种原型框架已通——新增只是加行数 + 蓝图子类，不碰 BT 框架）。

**验收标准**：
- [ ] `DT_Enemy` 含 10-15 行，每个原型至少 2 个变体
- [ ] 同原型两个变体（如玄龟 vs 石敢当）外观明显不同、数值微调（如玄龟更慢但盾更厚）
- [ ] 规则生效时，屏幕边缘/四角出现镜面裂纹流光（持续 ~1s），颜色与规则类型对应
- [ ] Boss 获得 AI 动态技能时，Boss 血条旁出现镜面符文
- [ ] 所有 AI 视觉反馈可切换关闭（Debug 开关，方便调平衡时不受干扰）

**负责人**：程序（视觉反馈管理器 + 敌人数值调试）+ 美术（剩余异兽模型/Material/变体外观 + 镜面特效素材）

---

**====== Sprint 5 完成标志 ======**
**"AI 会说话 + 内容量充足 + 有视觉风格的" Demo。LLM 可开关，台词有人格，10+ 异兽，镜面视觉语言统一。**

---

## S6-F1：镜界时间轴 + Director 开/关对照开关

**玩家价值**：打完一局后，看到一条完整的时间轴——白泽在第几层看到了什么，做了什么决定，我怎么回应的。我还能关掉 Director 打同一局，直观感受"有 AI 和没 AI 的区别"。

**功能描述**：每局游戏结束时，`UMirrorTimeline` 读取每层的 `FDirectorDecision` + `FFloorBehaviorSnapshot`，生成一条可回放的时间轴 UI。`UDirectorToggle` 在主菜单提供"AI Director 开/关"选项（关=固定刷怪表、无规则修改、白泽不出现）。

**技术范围**：`UMirrorTimeline`、`WBP_MirrorTimeline` UMG、`UDirectorToggle`、`ASMGameMode`。

**输入**：每层的快照 + 导演决策（已存在，只需在每层结束后存进数组）。

**输出**：整局 AI 决策可视化 + 演示时可一键关掉 AI。

**依赖**：S4-F3（完整 6 层流程已通——时间轴只是把已有的每层决策串成 UI）。

**验收标准**：
- [ ] Run 结束结算界面，点击"镜界时间轴"标签看到每层的事件：绿点=我打得好、红点=AI 调整、蓝点=我换了 Build
- [ ] 时间轴可滚动/缩放（最多 6 层 2 房间没多少数据，但有数据骨架）
- [ ] 主菜单开启/关闭 Director → 关掉后打一局，刷怪权重固定、没有导演报告、没有白泽台词
- [ ] 同一角色同一层,开 Director 和关 Director 的遭遇配置明显不同（肉眼可辨）
- [ ] **这是答辩时最有说服力的功能，必须在 W7 前达到可录制视频的质量**

**负责人**：程序

---

## S6-F2：简版跨局记忆

**玩家价值**：第二局开局时，白泽说："你的弓使得纯熟——上回也是如此。"它真的记得我。这让我想再打一局看它还会说什么。

**功能描述**：`UMirrorMemory` 保存最近 5 局的简版画像到本地存档文件（`PlayerModel.json`，非 UE SaveGame，方便 Debug 时直接读）。不搞 Design Bible 的五层长期学习——MVP 只存：最近 3 局的主导原型、最常用 Build Tag、平均通关率。下一局开局时，如果检测到存档，白泽在欢迎词里引用上局画像。

**技术范围**：`UMirrorMemory`、`FSimpleMirrorMemory` 结构、JsonArchive。

**输入**：每局结束时的 `FPlayerProfile`。

**输出**：跨局记忆文件 + 下局开场白泽台词有记忆感。

**数据结构**：
```cpp
USTRUCT() struct FSimpleMirrorMemory { TArray<FString> RecentArchetypes; FString MostUsedBuildTag; int32 TotalRuns; int32 Wins; };
```

**依赖**：S4-F3（整局结束才能保存）、S5-F1（LLM 可据此生成更个性化的台词）。

**验收标准**：
- [ ] 打完一局后，`Saved/MirrorMemory/PlayerModel.json` 文件写入
- [ ] 下一局开始时白泽台词包含对上局风格的引用（如弓、近战、受击多等，LLM 生成的台词和记忆数据有因果对应）
- [ ] 关闭 Director 时不读跨局记忆
- [ ] 删除存档文件后行为如初（Reset）

**负责人**：程序

---

## S6-F3：全套平衡调参 + Bug 修复 + 演示录制

**玩家价值**：游戏真的能打、能通、不崩溃。Boss 不喷血秒人、6 层难度曲线平滑、30 分钟不拖也不赶。Trailer 和 Gameplay 视频能直接用于比赛。

**功能描述**：W8 只做三件事：(1)调 6 层 + Boss 的难度曲线；(2)修所有已知 Bug（崩溃、逻辑错误、UI 显示问题）；(3)录制 2 个视频——1 个 Trailer（1-3 分钟）、1 个 Gameplay 演示（展示"开/关 AI Director 的区别"）。

**技术范围**：`DT_*` 数值调参、Bug 单、OBS/录屏。

**验收标准**：
- [ ] F6 Boss 首次通关率 30-45%（合理难度），F1 通关率 100%（新手友好）
- [ ] 全程无崩溃、无"敌人卡墙不动了"、无"导演决策不生效"的 Bug
- [ ] Trailer 展示：2 角色、AI 观察→调整→报告→玩家换 Build→Boss 战
- [ ] Gameplay 视频展示：开 AI 和关 AI 的对比——同一层、同一个人打、不同体验
- [ ] 两个视频都无需后期配音/解说也能看懂（有 UI 文字 + 白泽台词）

**负责人**：程序（调参 + Bug + 引擎内录制「开/关 AI 对照」比赛视频，替代 bespoke Trailer）

---

**====== Sprint 6 完成标志 ======**
**比赛 Demo 冻结。可演示、可答辩、不崩溃。**

---

# 第三步：推荐开发顺序

对比你给的顺序(`玩家移动→战斗→敌人→一局→肉鸽成长→AI导演`)和 TDD 的实际架构,我做了调整：

```
你建议的:   移动 → 战斗 → 敌人 → 一局流程 → 成长 →  AI导演
我推荐的:   移动 → 战斗+敌人(同一 Sprint) → 多敌人+闪避+多房间
              → AI导演(本地版)★先于成长 → 完整6层+成长+Boss
              → LLM+内容 → 打磨
```

**核心分歧在于"成长"和"AI 导演"的先后顺序。我的理由是：**

1. **AI 导演是整个项目的最高风险项。** 它能不能跑、跑出来玩家能不能感知到,是"这个 Demo 是否成立"的唯一判据。**越早验证,越早决定要不要改方向。** 你把 AI 导演放在最后,等于把最大风险压在最后两周——万一不行就完了。

2. **成长系统是低风险的成熟玩法,** Pick 3 + 被动叠加, Roguelike 标准答案,不存在"做不出来"的问题。它可以往后挪。

3. **而且 AI 导演的逻辑里已经把"诱导玩家成长方向"写进去了**（导演给近战强化加权重）,所以先在 Sprint 3 做完本地导演, Sprint 4 做成长时可以直接对接——数据流更顺。

---

# 第四步：明确延后的功能

以下在 TDD/GDD/Design Bible 中有提及,但 **不在本 Spec 的 Sprint 1-6 中**:

| 功能 | 延后原因 | 延到何时 |
|---|---|---|
| 第 3 角色 | P3, 2 个角色已够展示"不同角色不同画像" | MVP 后 |
| 商店系统 | GDD v0.2 已砍, 2 人做不完 | V1.1 |
| 元素系统 | 同上 | V1.5 |
| 角色进化(刑天/饕餮等) | 同上 | V1.5 |
| 多 Boss | MVP 单 Boss 烛龙, 多一个 Boss 的工作量 ≈ 再来 2 周 | V1.5 |
| Design Bible 的五层 Mirror Cognitive Architecture | 正式版架构, MVP 一个简单置信度规则就够 | V2.0 |
| AI Economy(跨 Run 的生态平衡) | 需要大量数据与 Telemetry, MVP 没有玩家基数 | V3.0 |
| 实时 LLM(战斗中调) | 延迟毁掉动作手感, 且无必要(见 TDD §4) | 永远不做 |
| 程序化地形生成 | 2 人 8 周不可行,对核心卖点零贡献(见 TDD §5.5) | 永远不做 |
| 完整 GAS | TDD v0.2 已锁定为自研(见 TDD §5.2) | 永远不用 |
| 白泽全程配音 | 纯文字即可, 配音是打磨末位, 且比赛可能没耳机 | MVP 后 |
| A/B 测试框架 | 需要玩家基数 | V2.0 |
| 跨 Run 的 Persistent AI(长期学习) | Design Bible V3.0 目标 | V3.0 |

**原则:凡是"对 AI 动态调整卖点"不直接贡献、且需要 >2 人周的,一律延后。**

---

# 第五步：Sprint Backlog

每个 Sprint 时长 = 2 周（对齐 TDD §6 的 8 周计划）。

---

## Sprint 1（Week 1-2）：Twin-Stick Combat 切片

**Sprint 目标**：一个测试房间, 一个角色, 能用 WASD+鼠标移动/瞄准/攻击, 有一种敌人能发现你、追你、打你、被你杀死。HP 归零死亡后重新开始。**做完这个,你手里有一个能"玩"的东西。**

| Spec | 名称 | 负责人 | 验收摘要 |
|---|---|---|---|
| S1-F1 | Twin-Stick 移动+瞄准 | 程序 | WASD 移, 鼠标瞄, 朝向与移动解耦 |
| S1-F2 | 基础近战攻击 | 程序 | 扇形判定 + 伤害数字 + Hit Stop |
| S1-F3 | Grunt 敌人 | 程序 + AI美术 | 发现-追击-攻击-死亡 |
| S1-F4 | 俯视角相机 + HP/死亡 | 程序 | 平滑跟随 + HP 条 + 死亡黑屏 |
| S1-F5 | Sprint 1 集成 | 程序 + AI美术 | 一个测试房间可完整打怪-清空-传送 |

**Sprint 1 Done = 可玩构建：一个房间、一个角色、一种敌人、能打能死。**

---

## Sprint 2（Week 3-4）：敌人多样化 + 闪避 + 多房间

**Sprint 目标**：战斗变得有深度——闪避、近战/远程切换、4 种行为截然不同的敌人、波次刷怪有节奏、两个房间串起来。**一个不无聊的战斗体验。**

| Spec | 名称 | 负责人 | 验收摘要 |
|---|---|---|---|
| S2-F1 | 闪避系统 | 程序 | 无敌帧 0.25s + 冷却 0.8s + 取消攻击 |
| S2-F2 | 武器切换 + 弓 | 程序 | 空格切 剑/弓 + 发射投射物 |
| S2-F3 | 4 种敌人原型 | 程序 + AI美术 | Grunt/Tank/Rush/Shooter 行为明显不同 |
| S2-F4 | 遭遇系统 | 程序 | 波次 + Budget + 清空判定 |
| S2-F5 | 2 房间模板 + 流式加载 | 程序 + AI美术 | 开阔/狭窄房 + 传送门过渡 |

**Sprint 2 Done = 可玩构建：一个角色、闪避、弓+剑、4 种怪、波次节奏、两个房间串起来。**

---

## Sprint 3（Week 5-6）：AI Director 本地版（★全项目最重要 Sprint）

**Sprint 目标**：**打完一层后 AI 分析我的打法,下一层敌人和规则因我而变。** 这是项目的"垂直线"——如果这个 Sprint 结束时 AI 调整让玩家感觉不到,立刻改方案。

| Spec | 名称 | 负责人 | 验收摘要 |
|---|---|---|---|
| S3-F1 | EventBus + BehaviorRecorder | 程序 | 五维 14 项埋点正确采集 |
| S3-F2 | ProfileAnalyzer | 程序 | 五维 0-100 + 置信度, 可单元测试 |
| S3-F3 | **LocalDirectorCore** | **程序** | 画像→决策→护栏, **远程/近战玩家得到不同第二层** |
| S3-F4 | RuleManager + 导演报告 UI | 程序 + AI美术 | 规则卡显示 + 全局 Effect 生效 |
| S3-F5 | 2 层完整 Demo 集成 | 程序 | 完整闭环可玩, 第二层因打法而异 |

**Sprint 3 Done = "AI 观察我、告诉我它看到了什么、下一层真的不一样了"——AI Native 成立。**

---

## Sprint 4（Week 7）：完整 Run + 成长 + Boss

**Sprint 目标**：30 分钟完整一局, 含 2 角色 + 6 层 + Pick 3 + Boss。这是项目的"内容下限"——即使 LLM 来不及集成,这个版本可以参赛。

| Spec | 名称 | 负责人 | 验收摘要 |
|---|---|---|---|
| S4-F1 | Pick 3 + 被动叠加 | 程序 | 每层选 1, 永久生效, AI 诱导权重生效 |
| S4-F2 | 方士角色 | 程序 + AI美术 | 远程法术, 低血高移速 |
| S4-F3 | 6 层流程 + 烛龙 Boss | 程序 + AI美术 | 30min 完整格, 3 阶段 + 动态技能 |

**Sprint 4 Done = 完整的 30 分钟 Roguelike Demo, 本地 AI Director 全功能, 没有 LLM 但可参赛。**

---

## Sprint 5（Week 8 前半）：LLM + 内容 + 视觉

**Sprint 目标**：白泽会说话了——LLM 集成, 人格化台词, 导演报告精修, 敌人数补满, AI 视觉反馈上。**有"人格"的 Demo。**

| Spec | 名称 | 负责人 | 验收摘要 |
|---|---|---|---|
| S5-F1 | GameNPC 集成 + 三级兜底 | 程序 | LLM 可开关, JSON 校验, 断网可玩 |
| S5-F2 | 白泽台词 + 导演报告精修 | 程序 + AI美术 | 每局台词不同, 山海经文风, UI 美观 |
| S5-F3 | 10-15 种敌人 + AI 视觉反馈 | 程序 + AI美术 | 镜面裂纹/符文统一视觉语言 |

**Sprint 5 Done = "AI 有话说 + 内容丰富 + 视觉风格统一"的 Demo。**

---

## Sprint 6（Week 8 后半）：打磨 + 演示 + 交付

**Sprint 目标**：Bug 修完, 平衡调好, 时间轴 + Director 开关做成, 两个视频录制。比赛材料全部输出。

| Spec | 名称 | 负责人 | 验收摘要 |
|---|---|---|---|
| S6-F1 | 镜界时间轴 + Director 开关 | 程序 | 赛后回放 + 答辩演示神器 |
| S6-F2 | 简版跨局记忆 | 程序 | 白泽"记得上局的我" |
| S6-F3 | 平衡调参 + Bug + 视频 | 程序 + AI美术 | 不崩溃 + 不难到打不过 + Trailer |

**Sprint 6 Done = 比赛 Demo 冻结, 材料齐全。**

---

## 每 Sprint 结束时的强制动作

1. **可玩构建**：从头到尾不崩溃, 发给朋友/同事试玩, 不看说明书能不能懂
2. **10 分钟回顾**：哪些 Spec 没做完 → 马上挪到下一 Sprint 还是砍掉?
3. **Git tag**：`sprint-1-done`, `sprint-2-done`... 便于回滚

---

## 风险评估(按 Sprint)

| Sprint | 最大风险 | 应对 |
|---|---|---|
| S1 | 战斗手感不及格（动作类游戏手感是最主观的） | 第一周就把战斗切片发给 2-3 个朋友盲测, 不要自己觉得好 |
| S2 | BT 复杂度超预期（4 种原型的行为差异如果写出 4 棵不同的树, 维护成本爆炸） | 严格"一棵共享树,差异只在攻击节点"纪律 |
| S3 | **AI 调整玩家感觉不到** | 如果 S3 结束朋友试玩说"第二层和第一层差不多", 立即扩大调整幅度 + 增强导演报告的视觉冲击 |
| S4 | Boss 做不完 / 美术内容积压 | Boss 先做 2 阶段（裁过渡阶段）, 房间模板量不够就 AI 多换刷怪配置少换模板 |
| S5 | GameNPC API 不稳定 / 延迟 >5s | LLM 默认关, 展示时开; 兜底确保任何时候都能玩 |
| S6 | 调参陷入死循环 | 设 Dead Stop: W8 最后 3 天不再调, 只修崩溃级 Bug |

---

## 附：Spec 编号与 TDD 章节对应(便于查)

| Spec | TDD 对应 | Spec | TDD 对应 |
|---|---|---|---|
| S1-F1 | §2.2 战斗(移动部分) | S3-F3 | §3.3-3.5 导演核心 |
| S1-F2 | §2.2 战斗(攻击判定) | S3-F4 | §2.2(规则Effect) + §8 AI透明性 |
| S1-F3 | §2.5 敌人(原型基础) | S3-F5 | §2.7 关卡生成(层过渡) |
| S1-F4 | §2.1 玩家(相机+HP) | S4-F1 | §2.6 强化系统 |
| S1-F5 | §2.7 关卡(单房) | S4-F2 | §2.3 角色系统 |
| S2-F1 | §2.2 战斗(闪避) | S4-F3 | §2.7(6层) + §14(Design Bible Boss) |
| S2-F2 | §2.4 武器系统 | S5-F1 | §3.4-3.5 LLM 接口 + §16 技术架构 |
| S2-F3 | §2.5 敌人(全部原型) | S5-F2 | §8 透明性 + §15(Design Bible) |
| S2-F4 | §2.2 遭遇系统 | S5-F3 | §2.5 敌人(数量) + §8 AI视觉 |
| S2-F5 | §2.7 房间模板 | S6-F1 | §8 时间轴 + §18(Design Bible 评估) |
| S3-F1 | §3.1 行为采集 | S6-F2 | §3.2(跨局) + §12(Design Bible 认知) |
| S3-F2 | §3.2 画像分析 | S6-F3 | §6(平衡) + §21(Design Bible QA) |
