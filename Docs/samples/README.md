# 样例数据

## `DecisionLog_Sample.json`（待补）

一局的完整决策日志，格式契约见
[`SHMDecisionLogFormat.h`](../../UnrealProject/Source/ShanHaiMirror/Director/SHMDecisionLogFormat.h)。

**这份文件必须来自真实对局导出，不手工编造**——它的价值就在于是真的。

生成方式（任选）：

- 打完整一局（3 层），结束时自动导出到 `UnrealProject/Saved/DecisionLogs/Run_*.json`
- 或游戏中控制台执行 `SHM.ExportDecisionLog`，导出到 `Saved/DecisionLogs/Manual_*.json`

拿到后复制到本目录即可。文件已是 UTF-8，可直接被任意 JSON 工具读取。

### 看这份文件的重点

每层的 `rawIntent` 与 `decision` 并排对比：

- `rawIntent.ruleIntents` —— **护栏前**，只有 `tag` 和 `level`，**没有任何数值**
- `decision.ruleModifiers` —— **护栏后**，出现 `multiplier`

数值只在护栏之后产生，这条架构主张在数据里肉眼可见。

`validation.violations` 记录哪道护栏拦下了什么，`trace` 记录这条决策出自
Local / Llm / Replay、是否降级、耗时多久。
