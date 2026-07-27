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
| 运行时间 | 2026-07-27 21:57 |
| 结果 | **52 / 52 通过** |
| 退出码 | 0 |
| 运行依赖 | 无需 World / PIE / 网络（被测对象为纯函数/纯逻辑）|

覆盖范围：画像分析器（链路②）· 规则解析器（链路⑥）· 四道护栏含分道归属（链路⑤）· 三个 Provider（链路④）· Intent JSON 解析 · Prompt 构建 · 遭遇抽样 · 武器切换。

| 结果 | 测试 |
|---|---|
| PASS | `SHM.Combat.Encounter.BuildWave_RespectsThreatBudget` |
| PASS | `SHM.Combat.Encounter.Pick_DegenerateInputsSafe` |
| PASS | `SHM.Combat.Encounter.Pick_DeterministicBuckets` |
| PASS | `SHM.Combat.Weapon.Switch_NoSubWeaponIsNoOp` |
| PASS | `SHM.Combat.Weapon.Switch_SwapsActiveWeapon` |
| PASS | `JsonIntent.HallucinatedTag_ParsedThenGuarded` |
| PASS | `JsonIntent.Malformed_FailsSafely` |
| PASS | `JsonIntent.MissingFields_UsesSafeDefaults` |
| PASS | `JsonIntent.NumericFields_AreDiscarded` |
| PASS | `JsonIntent.RoundTrip_PreservesIntent` |
| PASS | `JsonIntent.Valid_ParsesAllFields` |
| PASS | `Llm.Endpoint_IsConfigurable` |
| PASS | `Llm.NoApiKey_FailsImmediatelyForDegrade` |
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
| PASS | `Prompt.RequestBody_IsValidOpenAiShape` |
| PASS | `Prompt.SameContext_SamePrompt` |
| PASS | `Prompt.System_ForbidsNumericOutput` |
| PASS | `Prompt.User_InjectsCandidatesAndBudget` |
| PASS | `Replay.BadScripts_FailSafely` |
| PASS | `Replay.DefaultScript_LoadsAndProducesValidIntents` |
| PASS | `Replay.MissingFloor_ReturnsEmptyForDegrade` |
| PASS | `Replay.SameFloor_AlwaysSameIntent` |
| PASS | `RuleResolver.CsvFile_LoadsAndResolves` |
| PASS | `RuleResolver.KnownRule_MapsToMultiplier` |
| PASS | `RuleResolver.UnknownRule_ReturnsInvalidNoCrash` |
| PASS | `Validator.Budget_SumCostWithinBudget` |
| PASS | `Validator.Conflict_ExclusivePairRejected` |
| PASS | `Validator.Fairness_HeavyNeedsConfidence` |
| PASS | `Validator.Fairness_ThirdConsecutiveFloorRejected` |
| PASS | `Validator.MultiViolation_AttributedToCorrectGuards` |
| PASS | `Validator.Schema_TagsMustBeWhitelisted` |
| PASS | `Validator.Schema_WeightsMustSumToOne` |
| PASS | `Validator.ValidIntent_PassesAllFourGates` |

测试源码位于 `UnrealProject/Source/ShanHaiMirror/Tests/`，命名空间 `SHM.Director`。

> 注：DirectorCore 编排层（③⑤⑥串联）经由控制台命令 `SHM.DumpDecision` 在
> 真实 GameInstance 中验证（headless 运行日志），未做独立单测——其逻辑是对
> 已单测组件的顺序调用。
