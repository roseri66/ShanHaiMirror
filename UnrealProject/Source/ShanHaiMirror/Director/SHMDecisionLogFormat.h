#pragma once

#include "CoreMinimal.h"

// ============================================================================
// 决策日志 JSON 格式契约（P3）—— 一次定死，三方共用
//
// 这份格式有三个消费方，任何一方擅自改字段名都会悄悄拆掉另外两方：
//   ① FSHMReplayProvider  读它复现决策（回放脚本 = 本格式的子集）
//   ② 决策回放 UI          渲染它（下一阶段）
//   ③ 可能的后端服务       落库（若做）
//
// 因此：**字段名只在本文件出现一次**，所有读写方引用这里的常量，不写字符串字面量。
// 顶层第一个字段永远是 schemaVersion——格式演进时读取方据此决定怎么解析。
//
// 顶层结构（一局一个文件）：
// {
//   "schemaVersion": 1,
//   "runId": "...", "startedAt": "...", "totalFloors": 3,
//   "floors": [{
//     "floorIndex": 1,
//     "snapshot":   { 行为快照 },
//     "profile":    { 五维画像 },
//     "context":    { "challengeBudget": 55, "availableRules": [...] },
//     "rawIntent":  { 护栏前 · Provider 原样输出 },
//     "validation": { "valid": true, "violations": [{"guard":"Budget","detail":"..."}] },
//     "decision":   { 护栏后 · 含 Multiplier 数值 },
//     "trace":      { "providerId":"Llm", "degraded":false, "elapsedMs":1240 }
//   }]
// }
// ============================================================================
namespace SHMLogFormat
{
	// 格式版本。**改字段语义/删字段时必须 +1。**
	inline constexpr int32 SchemaVersion = 1;

	// --- 顶层 ---
	inline const TCHAR* Key_SchemaVersion = TEXT("schemaVersion");
	inline const TCHAR* Key_RunId         = TEXT("runId");
	inline const TCHAR* Key_StartedAt     = TEXT("startedAt");
	inline const TCHAR* Key_TotalFloors   = TEXT("totalFloors");
	inline const TCHAR* Key_Floors        = TEXT("floors");

	// --- 层条目 ---
	inline const TCHAR* Key_FloorIndex = TEXT("floorIndex");
	inline const TCHAR* Key_Snapshot   = TEXT("snapshot");
	inline const TCHAR* Key_Profile    = TEXT("profile");
	inline const TCHAR* Key_Context    = TEXT("context");
	inline const TCHAR* Key_RawIntent  = TEXT("rawIntent");
	inline const TCHAR* Key_Validation = TEXT("validation");
	inline const TCHAR* Key_Decision   = TEXT("decision");
	inline const TCHAR* Key_Trace      = TEXT("trace");

	// --- Intent（LLM 的输出契约；**这里没有任何数值字段，是刻意的**，见 D-15）---
	inline const TCHAR* Key_ChallengeLevel = TEXT("challengeLevel");
	inline const TCHAR* Key_EnemyWeights   = TEXT("enemyWeights");
	inline const TCHAR* Key_RuleIntents    = TEXT("ruleIntents");
	inline const TCHAR* Key_Narration      = TEXT("narration");
	inline const TCHAR* Key_Reason         = TEXT("reason");
	inline const TCHAR* Key_Tag            = TEXT("tag");
	inline const TCHAR* Key_Level          = TEXT("level");

	// --- Decision 独有（护栏后才有的数值）---
	inline const TCHAR* Key_RuleModifiers = TEXT("ruleModifiers");
	inline const TCHAR* Key_Multiplier    = TEXT("multiplier");
	inline const TCHAR* Key_Cost          = TEXT("cost");

	// --- Validation ---
	inline const TCHAR* Key_Valid      = TEXT("valid");
	inline const TCHAR* Key_Violations = TEXT("violations");
	inline const TCHAR* Key_Guard      = TEXT("guard");
	inline const TCHAR* Key_Detail     = TEXT("detail");

	// --- Trace ---
	inline const TCHAR* Key_ProviderId    = TEXT("providerId");
	inline const TCHAR* Key_Degraded      = TEXT("degraded");
	inline const TCHAR* Key_DegradeReason = TEXT("degradeReason");
	inline const TCHAR* Key_ElapsedMs     = TEXT("elapsedMs");
}
