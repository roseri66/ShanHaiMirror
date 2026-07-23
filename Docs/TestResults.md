# 单元测试结果

> 本文件由 `Saved/Logs/AutoTest.log` 生成，非手写。
> `Saved/` 被 gitignore 且重跑即覆盖，故在此留存一份可追溯的证据。

## 复现方式

```bash
"<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "<项目>/UnrealProject/ShanHaiMirror.uproject" \
  -ExecCmds="Automation RunTests SHM.Director;quit" \
  -unattended -nopause -nosplash -nullrhi \
  -stdout -AbsLog="<项目>/UnrealProject/Saved/Logs/AutoTest.log"
```

判定成功看这一行：`**** TEST COMPLETE. EXIT CODE: 0 ****`
（退出码非 0 即有测试未通过；这是测试失败的正常表现，不是启动失败）。

## 最近一次运行

| 项 | 值 |
|---|---|
| 运行时间 | 2026-07-23 22:51 |
| 结果 | **30 / 30 通过** |
| 退出码 | 0 |
| 运行依赖 | 无需 World / PIE / 网络（被测对象为纯函数/纯逻辑）|

覆盖范围：画像分析器（链路②）· 规则解析器（链路⑥）· 四道护栏（链路⑤）· 本地 Provider（链路④）。

| 结果 | 测试 |
|---|---|
| PASS | `LocalProvider.HighPressure_GetsRecovery` |
| PASS | `LocalProvider.LowConfidence_OnlyLightRules` |
| PASS | `LocalProvider.RangerProfile_GetsCounterWeights` |
| PASS | `LocalProvider.VanguardProfile_GetsCounterWeights` |
| PASS | `ProfileAnalyzer.Analyze_PopulatesAllFiveDimensions` |
| PASS | `ProfileAnalyzer.ArchetypeChange_ConfidenceResets` |
| PASS | `ProfileAnalyzer.BalancedWeapons_NoPrimaryBuild` |
| PASS | `ProfileAnalyzer.CombatEfficiency_FastAndUnhurtIsHigh` |
| PASS | `ProfileAnalyzer.CombatEfficiency_NoRoomsIsNeutral` |
| PASS | `ProfileAnalyzer.CombatEfficiency_SlowAndBeatenIsLow` |
| PASS | `ProfileAnalyzer.EmptyHistory_InitialConfidence` |
| PASS | `ProfileAnalyzer.ResourceSurplus_InertWithoutItemSystem` |
| PASS | `ProfileAnalyzer.SameArchetype_ConfidenceGrows` |
| PASS | `ProfileAnalyzer.SingleWeapon_FullConcentration` |
| PASS | `ProfileAnalyzer.StrategySwitch_ActiveSwitchingIsHigh` |
| PASS | `ProfileAnalyzer.StrategySwitch_NoCombatIsNeutral` |
| PASS | `ProfileAnalyzer.StrategySwitch_OneTrickIsZero` |
| PASS | `ProfileAnalyzer.SurvivalPressure_EventsAccumulate` |
| PASS | `ProfileAnalyzer.SurvivalPressure_NoEventsIsZero` |
| PASS | `ProfileAnalyzer.ZeroAttack_NoDivideByZero` |
| PASS | `RuleResolver.CsvFile_LoadsAndResolves` |
| PASS | `RuleResolver.KnownRule_MapsToMultiplier` |
| PASS | `RuleResolver.UnknownRule_ReturnsInvalidNoCrash` |
| PASS | `Validator.Budget_SumCostWithinBudget` |
| PASS | `Validator.Conflict_ExclusivePairRejected` |
| PASS | `Validator.Fairness_HeavyNeedsConfidence` |
| PASS | `Validator.Fairness_ThirdConsecutiveFloorRejected` |
| PASS | `Validator.Schema_TagsMustBeWhitelisted` |
| PASS | `Validator.Schema_WeightsMustSumToOne` |
| PASS | `Validator.ValidIntent_PassesAllFourGates` |

测试源码位于 `UnrealProject/Source/ShanHaiMirror/Tests/`，命名空间 `SHM.Director`。

> 注：DirectorCore 编排层（③⑤⑥串联）经由控制台命令 `SHM.DumpDecision` 在
> 真实 GameInstance 中验证（headless 运行日志），未做独立单测——其逻辑是对
> 已单测组件的顺序调用。
