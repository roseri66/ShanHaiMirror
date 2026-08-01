#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

class FJsonObject;

// ============================================================================
// 上行请求 JSON 格式契约（D-23）—— 客户端 → DirectorService
//
// 与 SHMDecisionLogFormat.h 的关系：**两份契约，不是一份。**
//   · 决策日志答的是「这一层最后发生了什么」，按"一层的完整记录"组织
//   · 本契约答的是「决策需要哪些输入」，按 FDirectorContext 的七个字段组织
// 两者结构不同构，把日志的 context 块直接当请求体发出去，服务端会拿不到
// 画像、候选原型和历史——Fairness 与 Budget 在服务端侧根本无从判断。
//
// 规矩完全照抄日志契约（同一批教训，没有理由在这里重犯）：
//   ① **字段名只在本文件出现一次**，读写方引用常量，不写字符串字面量
//   ② 顶层第一个字段永远是 schemaVersion
//   ③ 枚举/标签一律写**裸值名**，不带 C++ 类型前缀
//
// ---------------------------------------------------------------------------
// ⚠️ 踩坑 #22 在上行契约上会原封不动地重演一次。
//
// 那次是 `UEnum::GetValueAsString()` 把 "ESHMGuardrail::Conflict" 写进了下行日志，
// C++ 类型名泄漏进跨语言契约。上行的消费方是 **Java**，同样解析不出 `ESHM` 前缀。
// 所以这里同样禁用为调试设计的转换函数（GetValueAsString / ToString / %s），
// 标签一律走 `GetTagName().ToString()` 拿完整 Tag 名。
// ---------------------------------------------------------------------------
//
// 请求体结构（一层一次调用）：
// {
//   "schemaVersion": 1,
//   "runId": "20260730_143012_ab12",
//   "floorIndex": 1,
//   "totalFloors": 3,
//   "challengeBudget": 55,
//   "profile": { 七维画像 },
//   "availableRules":      [{ "tag": "Rule.Ammo", "level": "medium", "cost": 20 }],
//   "availableArchetypes": ["Enemy.Grunt", "Enemy.Tank"],
//   "decisionHistory":     [{ "floorIndex": 0, "ruleTags": ["Rule.Ammo"] }]
// }
//
// `availableArchetypes` 与 `decisionHistory` 是**必填**：没有它们，服务端连
// "这条规则上一层用过没有"都不知道，只能瞎选，选完必被客户端 Fairness 护栏拦下。
// ============================================================================
namespace SHMWireFormat
{
	// 上行格式版本。**改字段语义/删字段时必须 +1。**
	// 与日志的 SchemaVersion 各自独立演进——两份契约，两个版本号。
	inline constexpr int32 SchemaVersion = 1;

	// --- 请求顶层 ---
	inline const TCHAR* Key_SchemaVersion   = TEXT("schemaVersion");
	inline const TCHAR* Key_RunId           = TEXT("runId");
	inline const TCHAR* Key_FloorIndex      = TEXT("floorIndex");
	inline const TCHAR* Key_TotalFloors     = TEXT("totalFloors");
	inline const TCHAR* Key_ChallengeBudget = TEXT("challengeBudget");
	inline const TCHAR* Key_Profile         = TEXT("profile");
	inline const TCHAR* Key_AvailableRules  = TEXT("availableRules");
	inline const TCHAR* Key_AvailableArchetypes = TEXT("availableArchetypes");
	inline const TCHAR* Key_DecisionHistory = TEXT("decisionHistory");

	// --- 画像块（七维）---
	// ★ 这组常量同时是**决策日志 profile 块**的字段名真源：
	//   两份契约里画像的 JSON 形态本来就必须一致，共用一个 ProfileToJson 实现，
	//   结构上就不可能漂移。重构前它们是 RecordLogEntry 里的字符串字面量，
	//   而"同一个结构体的 JSON 形态存在两处"是第三处出现时三份不一致的起点。
	inline const TCHAR* Key_BuildConcentration = TEXT("buildConcentration");
	inline const TCHAR* Key_CombatEfficiency   = TEXT("combatEfficiency");
	inline const TCHAR* Key_ResourceSurplus    = TEXT("resourceSurplus");
	inline const TCHAR* Key_StrategySwitch     = TEXT("strategySwitch");
	inline const TCHAR* Key_SurvivalPressure   = TEXT("survivalPressure");
	inline const TCHAR* Key_Confidence         = TEXT("confidence");
	inline const TCHAR* Key_DominantArchetype  = TEXT("dominantArchetype");

	// ⚠️ **只进上行请求，不进决策日志**，故不在上面那组共用常量里。
	//
	// prompt 的「主力打法」一行读它（SHMPromptBuilder.cpp），LocalProvider 也用它
	// 判断远程/近战（SHMLocalProvider.cpp:65-66）—— 它有真实消费方。
	// prompt 搬到服务端后，不发这个字段，服务端就少一行 prompt，
	// 「原样搬迁」不成立。
	//
	// 不加进决策日志是为了保持日志格式与重构前逐字节一致：
	// 重构前的 RecordLogEntry 就没写它，现在补上等于改了日志格式，
	// 得动 schemaVersion 并同步改前端 TS 镜像 —— 而日志里没有它也不影响回放，
	// 那一屏不渲染主力打法。**两份契约本就允许有各自的字段，这是设计如此。**
	inline const TCHAR* Key_PrimaryBuildTags   = TEXT("primaryBuildTags");

	// ⚠️ 同样**只进上行请求，不进决策日志**。
	//
	// prompt 必须注入互斥信息，这是 2026-07-28 实测倒逼出的修正：
	// 不给它 LLM 只能盲选，DeepSeek 实测同时挑了「弹药↓ + 远程伤害↓」——
	// 对远程玩家是无解组合，被 Conflict 护栏拒、白白降级一次。
	// **候选集要给全，才叫「只给安全选项」。**
	//
	// 注意这不等于把互斥**判定**交给服务端：判定仍在客户端 Conflict 护栏
	// （D-23 的核心否决）。发过去只是让 LLM 别去选注定被拒的组合，
	// 是"减少无谓降级"，不是"服务端做校验"。两者区别要分清。
	inline const TCHAR* Key_ConflictsWith      = TEXT("conflictsWith");

	// --- 历史条目 ---
	// ⚠️ 只有这两个字段，是因为 FDirectorHistoryEntry 只有这两个字段。
	//   设计文档 §5.1 的示例里还画了 challengeLevel 与 playerAdapted，
	//   但结构体里**没有这两项数据**，也没有任何地方计算 playerAdapted。
	//   写进去只能填常量，那就是把"产品算不出来的状态"当成记录发出去——
	//   与踩坑 #23（四层日志）同一类问题，只是这次发生在请求体里。
	//   服务端若日后真需要它们，先在 UE 侧把数据源做出来，再同步加字段并 +SchemaVersion。
	inline const TCHAR* Key_RuleTags = TEXT("ruleTags");
}

// ============================================================================
// 结构体 ↔ JSON 的**唯一真源**。
//
// 决策日志（RecordLogEntry）与上行请求（FSHMRemoteProvider）都调用这里，
// 谁也不再自己手拼 FJsonObject。这是本次重构存在的全部理由：
// 同一个结构体的序列化只写一遍，第三个消费方出现时不会产生第三份形态。
//
// 纯函数，不碰世界不碰网络，可单测。
// ============================================================================
class SHANHAIMIRROR_API FSHMDirectorWire
{
public:
	// --- 共用块（日志与请求都用）---

	// FPlayerProfile → JSON。日志的 profile 块与请求的 profile 块由此保证同形。
	static TSharedPtr<FJsonObject> ProfileToJson(const FPlayerProfile& Profile);

	// 一条候选规则 → { "tag", "level", "cost" }
	static TSharedPtr<FJsonObject> AvailableRuleToJson(const FSHMAvailableRule& Rule);

	// --- 上行请求 ---

	// FDirectorContext + RunId → 完整请求体。
	// RunId 不在 Context 里（它是一局的标识，不是一次决策的输入），故单独传入。
	static TSharedPtr<FJsonObject> BuildIntentRequest(const FDirectorContext& Context,
		const FString& RunId);

	// 请求体 → 紧凑 JSON 文本（发 HTTP 用）
	static FString BuildIntentRequestString(const FDirectorContext& Context,
		const FString& RunId);

	// --- 决策日志复用 ---

	// 日志的 context 块 = { "challengeBudget", "availableRules" }。
	// **刻意只有两个字段**：日志记的是"当时允许挑什么"，
	// 完整输入以 profile / floorIndex 等形式存在同级，不重复。
	// 形状与上行请求不同构，这是设计如此，不是遗漏。
	static TSharedPtr<FJsonObject> LogContextToJson(const FDirectorContext& Context);
};
