# 单元测试结果

> 本文件由 `Saved/Logs/AutoTest.log` 生成，非手写。
> `Saved/` 被 gitignore 且重跑即覆盖，故在此留存一份可追溯的证据。

## 复现方式

```bash
"<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "<项目>/UnrealProject/ShanHaiMirror.uproject" \
  -ExecCmds="Automation RunTests SHM.;quit" \
  -unattended -nopause -nosplash -nullrhi \
  -stdout -AbsLog="<项目>/UnrealProject/Saved/Logs/AutoTest.log"
```

判定成功看这一行：`**** TEST COMPLETE. EXIT CODE: 0 ****`
（退出码非 0 即有测试未通过；这是测试失败的正常表现，不是启动失败）。

> 过滤器是 `SHM.` 而非 `SHM.Director`——后者会漏掉 `SHM.Combat.*` 的 5 个测试。

## 最近一次运行

| 项 | 值 |
|---|---|
| 运行时间 | 2026-08-14 14:11 |
| 结果 | **63 / 63 通过** |
| 退出码 | 0 |
| 运行依赖 | 无需 World / PIE / 网络（被测对象为纯函数/纯逻辑）|

覆盖范围：画像分析器（链路②）· 规则解析器（链路⑥）· 四道护栏（链路⑤）·
本地 Provider（链路④）· **三级降级（链路④的失败路径）**。

| 结果 | 测试（路径已去掉公共前缀 `SHM.`） |
|---|---|
| PASS | `Combat.Encounter.BuildWave_RespectsThreatBudget` |
| PASS | `Combat.Encounter.Pick_DegenerateInputsSafe` |
| PASS | `Combat.Encounter.Pick_DeterministicBuckets` |
| PASS | `Combat.Weapon.Switch_NoSubWeaponIsNoOp` |
| PASS | `Combat.Weapon.Switch_SwapsActiveWeapon` |
| PASS | `Director.DecisionLog.Export_IsUtf8AndParsable` |
| PASS | `Director.Degrade.Callback_FiresExactlyOnceOnEveryPath` |
| PASS | `Director.Degrade.GuardrailRejects_FallsBackAndKeepsRawIntent` |
| PASS | `Director.Degrade.ObserveFloor_BypassesProvider` |
| PASS | `Director.Degrade.ProviderFails_FallsBackToLocal` |
| PASS | `Director.JsonIntent.HallucinatedTag_ParsedThenGuarded` |
| PASS | `Director.JsonIntent.Malformed_FailsSafely` |
| PASS | `Director.JsonIntent.MissingFields_UsesSafeDefaults` |
| PASS | `Director.JsonIntent.NumericFields_AreDiscarded` |
| PASS | `Director.JsonIntent.RoundTrip_PreservesIntent` |
| PASS | `Director.JsonIntent.Valid_ParsesAllFields` |
| PASS | `Director.LocalProvider.HighPressure_GetsRecovery` |
| PASS | `Director.LocalProvider.LowConfidence_OnlyLightRules` |
| PASS | `Director.LocalProvider.RangerProfile_GetsCounterWeights` |
| PASS | `Director.LocalProvider.VanguardProfile_GetsCounterWeights` |
| PASS | `Director.ProfileAnalyzer.Analyze_PopulatesAllFiveDimensions` |
| PASS | `Director.ProfileAnalyzer.ArchetypeChange_ConfidenceResets` |
| PASS | `Director.ProfileAnalyzer.BalancedWeapons_NoPrimaryBuild` |
| PASS | `Director.ProfileAnalyzer.CombatEfficiency_FastAndUnhurtIsHigh` |
| PASS | `Director.ProfileAnalyzer.CombatEfficiency_NoRoomsIsNeutral` |
| PASS | `Director.ProfileAnalyzer.CombatEfficiency_SlowAndBeatenIsLow` |
| PASS | `Director.ProfileAnalyzer.EmptyHistory_InitialConfidence` |
| PASS | `Director.ProfileAnalyzer.ResourceSurplus_InertWithoutItemSystem` |
| PASS | `Director.ProfileAnalyzer.SameArchetype_ConfidenceGrows` |
| PASS | `Director.ProfileAnalyzer.SingleWeapon_FullConcentration` |
| PASS | `Director.ProfileAnalyzer.StrategySwitch_ActiveSwitchingIsHigh` |
| PASS | `Director.ProfileAnalyzer.StrategySwitch_NoCombatIsNeutral` |
| PASS | `Director.ProfileAnalyzer.StrategySwitch_OneTrickIsZero` |
| PASS | `Director.ProfileAnalyzer.SurvivalPressure_EventsAccumulate` |
| PASS | `Director.ProfileAnalyzer.SurvivalPressure_NoEventsIsZero` |
| PASS | `Director.ProfileAnalyzer.ZeroAttack_NoDivideByZero` |
| PASS | `Director.Remote.Disabled_FailsImmediatelyExactlyOnce` |
| PASS | `Director.Remote.FailureReason_IsDistinguishable` |
| PASS | `Director.Remote.IntentPath_MatchesServerContract` |
| PASS | `Director.Remote.IsAvailable_ReflectsConfigNotReachability` |
| PASS | `Director.Remote.ProviderName_IsRemote` |
| PASS | `Director.Remote.Timeout_LeavesRoomForServerUpstream` |
| PASS | `Director.Replay.BadScripts_FailSafely` |
| PASS | `Director.Replay.DefaultScript_LoadsAndProducesValidIntents` |
| PASS | `Director.Replay.MissingFloor_ReturnsEmptyForDegrade` |
| PASS | `Director.Replay.SameFloor_AlwaysSameIntent` |
| PASS | `Director.RuleResolver.CsvFile_LoadsAndResolves` |
| PASS | `Director.RuleResolver.KnownRule_MapsToMultiplier` |
| PASS | `Director.RuleResolver.UnknownRule_ReturnsInvalidNoCrash` |
| PASS | `Director.Validator.Budget_SumCostWithinBudget` |
| PASS | `Director.Validator.Conflict_ExclusivePairRejected` |
| PASS | `Director.Validator.Fairness_HeavyNeedsConfidence` |
| PASS | `Director.Validator.Fairness_ThirdConsecutiveFloorRejected` |
| PASS | `Director.Validator.MultiViolation_AttributedToCorrectGuards` |
| PASS | `Director.Validator.Schema_TagsMustBeWhitelisted` |
| PASS | `Director.Validator.Schema_WeightsMustSumToOne` |
| PASS | `Director.Validator.ValidIntent_PassesAllFourGates` |
| PASS | `Director.Wire.LogContext_StillExactlyTwoFields` |
| PASS | `Director.Wire.Profile_MatchesPreRefactorFieldNames` |
| PASS | `Director.Wire.Profile_SameShapeInLogAndRequest` |
| PASS | `Director.Wire.Request_CarriesAllSevenContextFields` |
| PASS | `Director.Wire.Request_HistoryOmitsFieldsWithNoDataSource` |
| PASS | `Director.Wire.Request_LeaksNoCppTypeNames` |

## 前端测试（WebReplay）

| 项 | 值 |
|---|---|
| 框架 | Vitest |
| 结果 | **84 个通过** |
| 复现 | `cd WebReplay && npm test` |
</content>
