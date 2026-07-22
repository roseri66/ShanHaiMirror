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
| 运行时间 | 2026-07-22 01:21 |
| 结果 | **16 / 16 通过** |
| 退出码 | 0 |
| 运行依赖 | 无需 World / PIE / 网络（被测对象为纯函数）|

| 结果 | 测试 |
|---|---|
| PASS | `Analyze_PopulatesAllFiveDimensions` |
| PASS | `ArchetypeChange_ConfidenceResets` |
| PASS | `BalancedWeapons_NoPrimaryBuild` |
| PASS | `CombatEfficiency_FastAndUnhurtIsHigh` |
| PASS | `CombatEfficiency_NoRoomsIsNeutral` |
| PASS | `CombatEfficiency_SlowAndBeatenIsLow` |
| PASS | `EmptyHistory_InitialConfidence` |
| PASS | `ResourceSurplus_InertWithoutItemSystem` |
| PASS | `SameArchetype_ConfidenceGrows` |
| PASS | `SingleWeapon_FullConcentration` |
| PASS | `StrategySwitch_ActiveSwitchingIsHigh` |
| PASS | `StrategySwitch_NoCombatIsNeutral` |
| PASS | `StrategySwitch_OneTrickIsZero` |
| PASS | `SurvivalPressure_EventsAccumulate` |
| PASS | `SurvivalPressure_NoEventsIsZero` |
| PASS | `ZeroAttack_NoDivideByZero` |

测试源码位于 `UnrealProject/Source/ShanHaiMirror/Tests/`，命名空间 `SHM.Director`。

> 注：本套测试覆盖画像分析器（链路第 ② 步）。
> 行为记录器（第 ① 步）尚无测试，导演核心（第 ③-⑥ 步）尚未实现。
