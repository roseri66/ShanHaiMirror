# 样例数据

## `DecisionLog_Sample.json`

**真实对局导出**（近战打法，LLM = DeepSeek `deepseek-chat`），仅替换了 runId 与时间戳。
格式契约见 [`SHMDecisionLogFormat.h`](../../UnrealProject/Source/ShanHaiMirror/Director/SHMDecisionLogFormat.h)。

这一局里 AI 导演做了什么：

| 层 | 决策来源 | 耗时 | 想改 → 实改 | 白泽说 |
|---|---|---|---|---|
| F0 | ObserveFloor | — | （不调整） | 第一层。我只是在看。 |
| F1 | **Llm** | 3721ms | MeleeDamage/medium → ×0.80 | 近战猛攻？试试砍穿这层铁壁。 |
| F2 | **Llm** | 2804ms | Heal + Ammo/medium → ×0.70 ×0.70 | 近战莽夫？试试远程消耗加治疗封锁，看你怎么近身。 |

台词是 LLM 现场生成的，针对玩家实际的近战打法——这一局三层护栏零拦截，决策直采。

> 自己再生成一份：打完整一局自动导出到 `UnrealProject/Saved/DecisionLogs/Run_*.json`，
> 或游戏中控制台 `SHM.ExportDecisionLog`。文件为 UTF-8，任意 JSON 工具可直读。

### 看这份文件的重点

每层的 `rawIntent` 与 `decision` 并排对比：

- `rawIntent.ruleIntents` —— **护栏前**，只有 `tag` 和 `level`，**没有任何数值**
- `decision.ruleModifiers` —— **护栏后**，出现 `multiplier`

数值只在护栏之后产生，这条架构主张在数据里肉眼可见。

`validation.violations` 记录哪道护栏拦下了什么，`trace` 记录这条决策出自
Local / Llm / Replay、是否降级、耗时多久。
