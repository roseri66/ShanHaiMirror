#include "SHMDirectorCore.h"
#include "SHMLocalProvider.h"
#include "SHMLlmProvider.h"
#include "SHMReplayProvider.h"
#include "SHMRuleResolver.h"
#include "SHMDecisionValidator.h"
#include "SHMJsonIntent.h"
#include "SHMDecisionLogFormat.h"
#include "Framework/SHMGameplayTags.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHMDirectorCore, Log, All);

void USHMDirectorCore::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 规则表：纯文本 CSV 入库（可 diff），运行时构建 DataTable。
	// 注意路径在 <项目>/Data/ 而不是 Content/——Content 被编辑器自动导入器监视，
	// 放 CSV 会弹"导入为 DataTable"提示，一旦导入就成了 CSV/uasset 双数据源（踩坑 #13）。
	// （若日后打包，需把 Data/ 加进 Additional Non-Asset Directories to Package。）
	const FString CsvPath = FPaths::ProjectDir() / TEXT("Data/RuleTable.csv");
	RuleTable = FSHMRuleResolver::LoadTableFromCsvFile(CsvPath, this);
	if (!RuleTable)
	{
		UE_LOG(LogSHMDirectorCore, Error,
			TEXT("规则表加载失败（%s）——导演将只能输出敌人配比，规则修改全部失效"), *CsvPath);
	}

	// 降级终点：永远在场，与主 Provider 是谁无关
	LocalFallback = MakeUnique<FSHMLocalProvider>();

	// --- Provider 选择 ---
	// 优先级：回放（显式开启，录屏/测试用）→ LLM（key 就位）→ 本地
	// 选谁都不影响下游：DirectorCore 只认 ISHMAIProvider，失败一律降级本地。
	const FString ReplayScript = FPlatformMisc::GetEnvironmentVariable(TEXT("SHM_REPLAY_SCRIPT"));
	if (!ReplayScript.IsEmpty())
	{
		TUniquePtr<FSHMReplayProvider> Replay = MakeUnique<FSHMReplayProvider>(ReplayScript);
		if (Replay->IsScriptLoaded())
		{
			Provider = MoveTemp(Replay);
		}
		else
		{
			UE_LOG(LogSHMDirectorCore, Warning, TEXT("回放脚本不可用，改用其它 Provider"));
		}
	}

	if (!Provider)
	{
		TUniquePtr<FSHMLlmProvider> Llm = MakeUnique<FSHMLlmProvider>();
		if (Llm->IsAvailable())
		{
			Provider = MoveTemp(Llm);
		}
	}

	if (!Provider)
	{
		Provider = MakeUnique<FSHMLocalProvider>();
	}

	UE_LOG(LogSHMDirectorCore, Log, TEXT("导演 Provider = %s"), *Provider->GetProviderName());

	ResetRun();
}

void USHMDirectorCore::ResetRun()
{
	DecisionHistory.Empty();
	LastDecision = FDirectorDecision();
	LogEntries.Empty();
	RunId        = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	RunStartedAt = FDateTime::UtcNow().ToIso8601();
}

float USHMDirectorCore::GetActiveRuleMultiplier(FGameplayTag RuleTag) const
{
	for (const FRuleModifier& Mod : LastDecision.RuleModifiers)
	{
		if (Mod.RuleTag == RuleTag)
		{
			return Mod.Multiplier;
		}
	}
	return 1.f;   // 未生效的规则 = 无修改
}

int32 USHMDirectorCore::ChallengeBudgetForFloor(int32 FloorIndex)
{
	// F0 = 0：第一层只观察不调整（预算为零 → Provider 一条规则都买不起，自然实现）
	// F1 = 30, F2 = 55：对齐 TDD §3.3 示例的预算曲线
	return FloorIndex <= 0 ? 0 : 5 + FloorIndex * 25;
}

FDirectorContext USHMDirectorCore::BuildContext(const FPlayerProfile& Profile, int32 FloorIndex) const
{
	// 链路第 ③ 步 CONSTRAIN：约束在这里收敛，Provider 拿到的候选集已是安全的
	FDirectorContext Ctx;
	Ctx.Profile         = Profile;
	Ctx.FloorIndex      = FloorIndex;
	Ctx.TotalFloors     = 3;                                  // DECISIONS D-02
	Ctx.ChallengeBudget = ChallengeBudgetForFloor(FloorIndex);
	Ctx.DecisionHistory = DecisionHistory;

	Ctx.AvailableArchetypes = {
		SHMTags::Enemy_Grunt.GetTag(), SHMTags::Enemy_Tank.GetTag(),
		SHMTags::Enemy_Rush.GetTag(),  SHMTags::Enemy_Shooter.GetTag() };

	if (RuleTable)
	{
		// 预过滤 ①：正常模式不开放 heavy（GDD §5.1 重度规则不在正常模式）
		// 预过滤 ②：已连续用满 MaxConsecutiveFloors 层的规则直接不进候选集——
		//           Provider 想选也选不到；Validator 的 Fairness 是它之上的第二道保险
		TSet<FGameplayTag> ExhaustedRules;
		if (DecisionHistory.Num() >= FSHMDecisionValidator::MaxConsecutiveFloors)
		{
			const int32 N = DecisionHistory.Num();
			for (const FGameplayTag& Tag : DecisionHistory[N - 1].AppliedRuleTags)
			{
				bool bInAll = true;
				for (int32 k = 2; k <= FSHMDecisionValidator::MaxConsecutiveFloors; ++k)
				{
					if (!DecisionHistory[N - k].AppliedRuleTags.Contains(Tag)) { bInAll = false; break; }
				}
				if (bInAll) { ExhaustedRules.Add(Tag); }
			}
		}

		RuleTable->ForeachRow<FSHMRuleRow>(TEXT("BuildContext"),
			[&Ctx, &ExhaustedRules](const FName&, const FSHMRuleRow& Row)
		{
			if (Row.Level == TEXT("heavy"))          { return; }
			if (ExhaustedRules.Contains(Row.RuleTag)) { return; }

			FSHMAvailableRule Avail;
			Avail.RuleTag       = Row.RuleTag;
			Avail.Level         = Row.Level;
			Avail.Cost          = Row.Cost;
			Avail.ConflictsWith = Row.ConflictsWith;
			Ctx.AvailableRules.Add(Avail);
		});
	}

	return Ctx;
}

// ============================================================================
//  观察层决策（F0 专用，代码强制，不进 Provider）
//
//  预算=0 只能挡住规则；挑战等级/配比/台词都出自 Provider 对画像的解读，
//  若画像携带数据（合成画像、未来的跨局记忆），Provider 照样会输出定向反制，
//  语言就在承诺一件首层不该做的事。设计不变量不能依赖「首层画像恰好为空」。
// ============================================================================
FDirectorDecision USHMDirectorCore::MakeObserveFloorDecision()
{
	FDirectorDecision Decision;
	Decision.ChallengeLevel = EChallengeLevel::Stable;
	Decision.EnemyWeights.Add(SHMTags::Enemy_Grunt.GetTag(),   0.55f);
	Decision.EnemyWeights.Add(SHMTags::Enemy_Tank.GetTag(),    0.15f);
	Decision.EnemyWeights.Add(SHMTags::Enemy_Rush.GetTag(),    0.15f);
	Decision.EnemyWeights.Add(SHMTags::Enemy_Shooter.GetTag(), 0.15f);
	Decision.NarrationLine = TEXT("第一层。我只是在看。");
	Decision.Reason        = TEXT("首层只观察：建立画像，不做任何调整。");
	Decision.Trace.ProviderId = TEXT("ObserveFloor");
	return Decision;
}

// ============================================================================
//  异步主入口 —— 三级降级链路
//
//    ① Provider（LLM/回放）失败   → 本地 Provider 重新决策
//    ② Provider 返回但护栏拒绝     → 本地 Provider 重新决策
//    ③ 本地也异常（不该发生）      → MakeSafeFallbackDecision
//
//  **每一级都留日志，且回调必定被调用一次**——调用方不必处理「没回调」的情况。
// ============================================================================
void USHMDirectorCore::DecideForFloorAsync(const FPlayerProfile& Profile, int32 FloorIndex,
	FSHMOnDecisionReady OnDecision)
{
	// 首层只观察
	if (FloorIndex <= 0)
	{
		const FDirectorDecision Decision = MakeObserveFloorDecision();

		FDirectorHistoryEntry Entry;
		Entry.FloorIndex = FloorIndex;
		DecisionHistory.Add(Entry);
		LastDecision = Decision;

		const FDirectorContext ObserveCtx = BuildContext(Profile, FloorIndex);
		RecordLogEntry(ObserveCtx, FDirectorIntent(), FValidationResult(), Decision);

		UE_LOG(LogSHMDirectorCore, Log, TEXT("F%d 观察层：\n%s"), FloorIndex, *DecisionToString(Decision));
		OnDecision.ExecuteIfBound(Decision);
		return;
	}

	if (!Provider)
	{
		OnDecision.ExecuteIfBound(MakeSafeFallbackDecision(TEXT("Provider 未初始化")));
		return;
	}

	// ③ CONSTRAIN
	const FDirectorContext Ctx = BuildContext(Profile, FloorIndex);
	const FString ProviderId   = Provider->GetProviderName();
	const double  StartTime    = FPlatformTime::Seconds();

	// ④ CHOOSE（异步；本地/回放会在此调用内立即回调）
	Provider->RequestIntentAsync(Ctx, FSHMOnIntentReady::CreateLambda(
		[this, Ctx, ProviderId, StartTime, OnDecision](const FDirectorIntent& Intent, bool bSuccess)
	{
		const float ElapsedMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);

		// --- 降级 ①：Provider 交不出结果（超时/HTTP 错/解析失败/脚本缺层）---
		if (!bSuccess)
		{
			UE_LOG(LogSHMDirectorCore, Warning,
				TEXT("[%s] 未能产出决策（耗时 %.0fms）——降级本地"), *ProviderId, ElapsedMs);

			const FDirectorIntent LocalIntent = LocalFallback->RequestIntent(Ctx);
			FinishDecision(Ctx, LocalIntent, ProviderId, true,
				FString::Printf(TEXT("%s 无可用输出"), *ProviderId), ElapsedMs, OnDecision);
			return;
		}

		FinishDecision(Ctx, Intent, ProviderId, false, FString(), ElapsedMs, OnDecision);
	}));
}

// ============================================================================
//  Intent 就绪后的收尾：⑤ 护栏 → ⑥ 查表 → 记历史/日志 → 回调
// ============================================================================
void USHMDirectorCore::FinishDecision(const FDirectorContext& Context, const FDirectorIntent& Intent,
	const FString& ProviderId, bool bDegraded, const FString& DegradeReason,
	float ElapsedMs, FSHMOnDecisionReady OnDecision)
{
	// ⑤ VALIDATE —— LLM 输出天然不可信；本地 Provider 理论必过，不过即为其 bug
	const FValidationResult Validation = FSHMDecisionValidator::Validate(Intent, Context);
	FDirectorIntent EffectiveIntent = Intent;
	bool    bFinalDegraded = bDegraded;
	FString FinalReason    = DegradeReason;

	if (!Validation.bValid)
	{
		for (const FSHMValidationViolation& Violation : Validation.Violations)
		{
			UE_LOG(LogSHMDirectorCore, Warning, TEXT("[%s] 决策被 %s 护栏拒绝：%s"),
				*ProviderId,
				*UEnum::GetDisplayValueAsText(Violation.Guard).ToString(),
				*Violation.Detail);
		}

		// --- 降级 ②：护栏拒绝 → 本地重新决策 ---
		EffectiveIntent = LocalFallback->RequestIntent(Context);
		bFinalDegraded  = true;
		FinalReason     = FString::Printf(TEXT("%s 输出被护栏拒绝（%d 项）"),
			*ProviderId, Validation.Violations.Num());

		// 本地产出也要过一遍护栏——不给任何 Provider 免检特权
		const FValidationResult LocalValidation =
			FSHMDecisionValidator::Validate(EffectiveIntent, Context);

		// --- 降级 ③：连本地都过不了（不该发生）→ 安全兜底 ---
		if (!LocalValidation.bValid)
		{
			UE_LOG(LogSHMDirectorCore, Error,
				TEXT("本地 Provider 的输出也未通过护栏——这是 Provider bug，启用安全兜底"));

			FDirectorDecision Safe = MakeSafeFallbackDecision(TEXT("本地决策亦被护栏拒绝"));
			Safe.Trace.ProviderId     = ProviderId;
			Safe.Trace.bDegraded      = true;
			Safe.Trace.DegradeReason  = FinalReason;
			Safe.Trace.ElapsedMs      = ElapsedMs;
			Safe.Trace.ViolationCount = Validation.Violations.Num();

			FDirectorHistoryEntry SafeEntry;
			SafeEntry.FloorIndex = Context.FloorIndex;
			DecisionHistory.Add(SafeEntry);

			LastDecision = Safe;
			RecordLogEntry(Context, Intent, Validation, Safe);
			OnDecision.ExecuteIfBound(Safe);
			return;
		}
	}

	// ⑥ RESOLVE —— 数值在此产生（全链路唯一产生点）
	FDirectorDecision Decision;
	Decision.ChallengeLevel = EffectiveIntent.ChallengeLevel;
	Decision.EnemyWeights   = EffectiveIntent.EnemyWeights;
	Decision.NarrationLine  = EffectiveIntent.Narration;
	Decision.Reason         = EffectiveIntent.Reason;

	for (const FRuleIntent& RuleIntent : EffectiveIntent.RuleIntents)
	{
		const FRuleModifier Resolved = FSHMRuleResolver::Resolve(RuleIntent, RuleTable);
		if (Resolved.RuleTag.IsValid())
		{
			Decision.RuleModifiers.Add(Resolved);
		}
		// 解析失败的规则被静默丢弃（Resolver 已打日志）——决策仍可用，只是少一条
	}

	// 溯源：谁出的、有没有降级、花了多久。**事后无法补造，必须此刻填**
	Decision.Trace.ProviderId     = ProviderId;
	Decision.Trace.bDegraded      = bFinalDegraded;
	Decision.Trace.DegradeReason  = FinalReason;
	Decision.Trace.ElapsedMs      = ElapsedMs;
	Decision.Trace.ViolationCount = Validation.Violations.Num();

	// 记历史：Fairness 护栏的输入
	FDirectorHistoryEntry Entry;
	Entry.FloorIndex = Context.FloorIndex;
	for (const FRuleModifier& Mod : Decision.RuleModifiers)
	{
		Entry.AppliedRuleTags.Add(Mod.RuleTag);
	}
	DecisionHistory.Add(Entry);

	LastDecision = Decision;
	RecordLogEntry(Context, Intent, Validation, Decision);   // 存的是**护栏前**的原始 Intent

	UE_LOG(LogSHMDirectorCore, Log, TEXT("F%d 决策完成（%s%s，%.0fms）：\n%s"),
		Context.FloorIndex, *ProviderId,
		bFinalDegraded ? TEXT(" · 已降级") : TEXT(""),
		ElapsedMs, *DecisionToString(Decision));

	OnDecision.ExecuteIfBound(Decision);
}

// ============================================================================
//  同步入口 —— 强制走本地，不发网络（控制台调试 / 单测用）
// ============================================================================
FDirectorDecision USHMDirectorCore::DecideForFloor(const FPlayerProfile& Profile, int32 FloorIndex)
{
	if (FloorIndex <= 0)
	{
		const FDirectorDecision Observe = MakeObserveFloorDecision();
		FDirectorHistoryEntry Entry;
		Entry.FloorIndex = FloorIndex;
		DecisionHistory.Add(Entry);
		LastDecision = Observe;
		UE_LOG(LogSHMDirectorCore, Log, TEXT("F%d 观察层：\n%s"), FloorIndex, *DecisionToString(Observe));
		return Observe;
	}

	if (!LocalFallback)
	{
		return MakeSafeFallbackDecision(TEXT("本地 Provider 未初始化"));
	}

	const FDirectorContext Ctx = BuildContext(Profile, FloorIndex);
	const FDirectorIntent Intent = LocalFallback->RequestIntent(Ctx);

	// 本地路径全程同步，回调必定在 FinishDecision 内立即触发
	FDirectorDecision Result;
	bool bGot = false;
	FinishDecision(Ctx, Intent, TEXT("Local"), false, FString(), 0.f,
		FSHMOnDecisionReady::CreateLambda([&Result, &bGot](const FDirectorDecision& D)
		{
			Result = D;
			bGot   = true;
		}));

	return bGot ? Result : MakeSafeFallbackDecision(TEXT("同步决策未回调"));
}

// ============================================================================
//  决策日志（P3 格式）—— 一层一条
// ============================================================================
void USHMDirectorCore::RecordLogEntry(const FDirectorContext& Context, const FDirectorIntent& RawIntent,
	const FValidationResult& Validation, const FDirectorDecision& Decision)
{
	using namespace SHMLogFormat;

	TSharedPtr<FJsonObject> Floor = MakeShared<FJsonObject>();
	Floor->SetNumberField(Key_FloorIndex, Context.FloorIndex);

	// --- 画像 ---
	TSharedPtr<FJsonObject> Profile = MakeShared<FJsonObject>();
	Profile->SetNumberField(TEXT("buildConcentration"), Context.Profile.BuildConcentration);
	Profile->SetNumberField(TEXT("combatEfficiency"),   Context.Profile.CombatEfficiency);
	Profile->SetNumberField(TEXT("resourceSurplus"),    Context.Profile.ResourceSurplus);
	Profile->SetNumberField(TEXT("strategySwitch"),     Context.Profile.StrategySwitch);
	Profile->SetNumberField(TEXT("survivalPressure"),   Context.Profile.SurvivalPressure);
	Profile->SetNumberField(TEXT("confidence"),         Context.Profile.Confidence);
	Profile->SetStringField(TEXT("dominantArchetype"),  Context.Profile.DominantArchetype.GetTagName().ToString());
	Floor->SetObjectField(Key_Profile, Profile);

	// --- 约束（LLM 当时能选的范围）---
	TSharedPtr<FJsonObject> CtxObj = MakeShared<FJsonObject>();
	CtxObj->SetNumberField(TEXT("challengeBudget"), Context.ChallengeBudget);
	TArray<TSharedPtr<FJsonValue>> AvailRules;
	for (const FSHMAvailableRule& Rule : Context.AvailableRules)
	{
		TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
		R->SetStringField(Key_Tag,   Rule.RuleTag.GetTagName().ToString());
		R->SetStringField(Key_Level, Rule.Level);
		R->SetNumberField(Key_Cost,  Rule.Cost);
		AvailRules.Add(MakeShared<FJsonValueObject>(R));
	}
	CtxObj->SetArrayField(TEXT("availableRules"), AvailRules);
	Floor->SetObjectField(Key_Context, CtxObj);

	// --- 护栏前的原始意图 ★ 本日志最值钱的字段 ---
	// 「Provider 想干什么」vs「最终允许它干什么」并排，是本项目最有说服力的一屏数据。
	// 不在这一刻存下，事后永远补不回来。
	Floor->SetObjectField(Key_RawIntent, FSHMJsonIntent::ToJsonObject(RawIntent));

	// --- 护栏判定（分道）---
	TSharedPtr<FJsonObject> Val = MakeShared<FJsonObject>();
	Val->SetBoolField(Key_Valid, Validation.bValid);
	TArray<TSharedPtr<FJsonValue>> Violations;
	for (const FSHMValidationViolation& V : Validation.Violations)
	{
		TSharedPtr<FJsonObject> VObj = MakeShared<FJsonObject>();
		VObj->SetStringField(Key_Guard,  UEnum::GetValueAsString(V.Guard));
		VObj->SetStringField(Key_Detail, V.Detail);
		Violations.Add(MakeShared<FJsonValueObject>(VObj));
	}
	Val->SetArrayField(Key_Violations, Violations);
	Floor->SetObjectField(Key_Validation, Val);

	// --- 护栏后的最终决策（含数值）---
	TSharedPtr<FJsonObject> Dec = MakeShared<FJsonObject>();
	Dec->SetStringField(Key_ChallengeLevel, FSHMJsonIntent::ChallengeLevelToString(Decision.ChallengeLevel));
	TSharedPtr<FJsonObject> Weights = MakeShared<FJsonObject>();
	for (const TPair<FGameplayTag, float>& Pair : Decision.EnemyWeights)
	{
		Weights->SetNumberField(Pair.Key.GetTagName().ToString(), Pair.Value);
	}
	Dec->SetObjectField(Key_EnemyWeights, Weights);
	TArray<TSharedPtr<FJsonValue>> Mods;
	for (const FRuleModifier& Mod : Decision.RuleModifiers)
	{
		TSharedPtr<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(Key_Tag,        Mod.RuleTag.GetTagName().ToString());
		M->SetStringField(Key_Level,      Mod.Level);
		M->SetNumberField(Key_Multiplier, Mod.Multiplier);   // 数值只在护栏后出现
		M->SetNumberField(Key_Cost,       Mod.Cost);
		Mods.Add(MakeShared<FJsonValueObject>(M));
	}
	Dec->SetArrayField(Key_RuleModifiers, Mods);
	Dec->SetStringField(Key_Narration, Decision.NarrationLine);
	Dec->SetStringField(Key_Reason,    Decision.Reason);
	Floor->SetObjectField(Key_Decision, Dec);

	// --- 溯源 ---
	TSharedPtr<FJsonObject> Trace = MakeShared<FJsonObject>();
	Trace->SetStringField(Key_ProviderId,    Decision.Trace.ProviderId);
	Trace->SetBoolField  (Key_Degraded,      Decision.Trace.bDegraded);
	Trace->SetStringField(Key_DegradeReason, Decision.Trace.DegradeReason);
	Trace->SetNumberField(Key_ElapsedMs,     Decision.Trace.ElapsedMs);
	Floor->SetObjectField(Key_Trace, Trace);

	LogEntries.Add(Floor);
}

bool USHMDirectorCore::ExportDecisionLog(const FString& AbsolutePath) const
{
	using namespace SHMLogFormat;

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(Key_SchemaVersion, SchemaVersion);   // 永远是第一个字段
	Root->SetStringField(Key_RunId,         RunId);
	Root->SetStringField(Key_StartedAt,     RunStartedAt);
	Root->SetNumberField(Key_TotalFloors,   LogEntries.Num());

	TArray<TSharedPtr<FJsonValue>> Floors;
	for (const TSharedPtr<FJsonObject>& Entry : LogEntries)
	{
		Floors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	Root->SetArrayField(Key_Floors, Floors);

	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	// **必须 UTF-8**：SaveStringToFile 遇到非 ASCII（我们的中文台词）默认写 UTF-16 LE，
	// UE 自己读没问题，但 JSON 交换标准是 UTF-8——网页端 fetch().json()、
	// Python json.load 都会直接失败。P3 承诺的「三方共用一份格式」在编码上也得成立。
	const bool bSaved = FFileHelper::SaveStringToFile(Out, *AbsolutePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogSHMDirectorCore, Log, TEXT("决策日志导出%s：%s（%d 层）"),
		bSaved ? TEXT("成功") : TEXT("失败"), *AbsolutePath, LogEntries.Num());
	return bSaved;
}

FDirectorDecision USHMDirectorCore::MakeSafeFallbackDecision(const FString& Reason) const
{
	FDirectorDecision Decision;
	Decision.ChallengeLevel = EChallengeLevel::Stable;
	Decision.EnemyWeights.Add(SHMTags::Enemy_Grunt.GetTag(), 1.0f);
	Decision.NarrationLine = TEXT("继续吧。我在看着。");
	Decision.Reason = FString::Printf(TEXT("[安全兜底] %s"), *Reason);
	return Decision;
}

FString USHMDirectorCore::DecisionToString(const FDirectorDecision& Decision)
{
	FString Out;
	Out += FString::Printf(TEXT("  挑战等级: %s\n"), *UEnum::GetDisplayValueAsText(Decision.ChallengeLevel).ToString());

	Out += TEXT("  敌人配比:");
	for (const TPair<FGameplayTag, float>& Pair : Decision.EnemyWeights)
	{
		Out += FString::Printf(TEXT(" %s=%.2f"), *Pair.Key.GetTagName().ToString(), Pair.Value);
	}
	Out += TEXT("\n");

	if (Decision.RuleModifiers.Num() == 0)
	{
		Out += TEXT("  规则: (无)\n");
	}
	for (const FRuleModifier& Mod : Decision.RuleModifiers)
	{
		Out += FString::Printf(TEXT("  规则: %s/%s ×%.2f (cost %d)\n"),
			*Mod.RuleTag.GetTagName().ToString(), *Mod.Level, Mod.Multiplier, Mod.Cost);
	}

	Out += FString::Printf(TEXT("  白泽: %s\n"), *Decision.NarrationLine);
	Out += FString::Printf(TEXT("  理由: %s"), *Decision.Reason);
	return Out;
}

// ============================================================================
//  控制台命令：SHM.DumpDecision [ranger|vanguard] [floor]
//  手喂画像跑完整链路——W2 的验收出口，也是 W3 前调参的工具
// ============================================================================
static FAutoConsoleCommandWithWorldAndArgs GDumpDecisionCmd(
	TEXT("SHM.DumpDecision"),
	TEXT("手喂画像跑完整决策链路。用法: SHM.DumpDecision [ranger|vanguard] [floor=2]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		// 失败必须出声——静默的提前返回会让"命令没生效"和"命令没执行"无法区分
		if (!World)
		{
			UE_LOG(LogSHMDirectorCore, Warning, TEXT("SHM.DumpDecision: 无 World 上下文"));
			return;
		}
		if (!World->GetGameInstance())
		{
			UE_LOG(LogSHMDirectorCore, Warning,
				TEXT("SHM.DumpDecision: World '%s' 无 GameInstance——需要在 PIE 运行中执行（先点 Play）"),
				*World->GetName());
			return;
		}
		USHMDirectorCore* Core = World->GetGameInstance()->GetSubsystem<USHMDirectorCore>();
		if (!Core)
		{
			UE_LOG(LogSHMDirectorCore, Warning, TEXT("SHM.DumpDecision: DirectorCore 子系统不存在"));
			return;
		}

		// 合成画像：ranger = 远程站桩，vanguard = 近战莽夫
		const bool bVanguard = Args.Num() > 0 && Args[0].Equals(TEXT("vanguard"), ESearchCase::IgnoreCase);
		const int32 Floor    = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 2;

		FPlayerProfile Profile;
		Profile.BuildConcentration = 90.f;
		Profile.CombatEfficiency   = 85.f;
		Profile.ResourceSurplus    = 50.f;
		Profile.StrategySwitch     = 5.f;
		Profile.SurvivalPressure   = 10.f;
		Profile.Confidence         = 0.9f;
		Profile.DominantArchetype  = bVanguard ? SHMTags::Archetype_Vanguard.GetTag()
		                                       : SHMTags::Archetype_Ranger.GetTag();
		Profile.PrimaryBuildTags   = { bVanguard ? SHMTags::Build_Melee.GetTag()
		                                         : SHMTags::Build_Ranged.GetTag() };

		// 同步版强制走本地，不发网络——控制台调试要的是即时结果
		const FDirectorDecision Decision = Core->DecideForFloor(Profile, Floor);
		UE_LOG(LogSHMDirectorCore, Display,
			TEXT("=== SHM.DumpDecision (%s, F%d) ===\n  当前 Provider: %s（本命令强制走 Local，不发网络）\n%s"),
			bVanguard ? TEXT("vanguard") : TEXT("ranger"), Floor,
			*Core->GetProviderName(),
			*USHMDirectorCore::DecisionToString(Decision));
	}));

// ============================================================================
//  控制台命令：SHM.DumpDecisionAsync —— 走真实 Provider（含 LLM），验证降级链路
// ============================================================================
static FAutoConsoleCommandWithWorldAndArgs GDumpDecisionAsyncCmd(
	TEXT("SHM.DumpDecisionAsync"),
	TEXT("走真实 Provider（可能发起 LLM 请求）跑一次决策。用法: SHM.DumpDecisionAsync [ranger|vanguard] [floor=2]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->GetGameInstance())
		{
			UE_LOG(LogSHMDirectorCore, Warning,
				TEXT("SHM.DumpDecisionAsync: 需要在 PIE 运行中执行（先点 Play）"));
			return;
		}
		USHMDirectorCore* Core = World->GetGameInstance()->GetSubsystem<USHMDirectorCore>();
		if (!Core) { return; }

		const bool bVanguard = Args.Num() > 0 && Args[0].Equals(TEXT("vanguard"), ESearchCase::IgnoreCase);
		const int32 Floor    = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 2;

		FPlayerProfile Profile;
		Profile.BuildConcentration = 90.f;
		Profile.CombatEfficiency   = 85.f;
		Profile.ResourceSurplus    = 50.f;
		Profile.StrategySwitch     = 5.f;
		Profile.SurvivalPressure   = 10.f;
		Profile.Confidence         = 0.9f;
		Profile.DominantArchetype  = bVanguard ? SHMTags::Archetype_Vanguard.GetTag()
		                                       : SHMTags::Archetype_Ranger.GetTag();
		Profile.PrimaryBuildTags   = { bVanguard ? SHMTags::Build_Melee.GetTag()
		                                         : SHMTags::Build_Ranged.GetTag() };

		UE_LOG(LogSHMDirectorCore, Display, TEXT("=== SHM.DumpDecisionAsync 发起（Provider=%s）==="),
			*Core->GetProviderName());

		Core->DecideForFloorAsync(Profile, Floor,
			USHMDirectorCore::FSHMOnDecisionReady::CreateLambda([](const FDirectorDecision& Decision)
		{
			UE_LOG(LogSHMDirectorCore, Display,
				TEXT("=== 决策返回 ===\n  出自: %s%s\n  耗时: %.0fms\n  护栏拦截: %d 项\n%s"),
				*Decision.Trace.ProviderId,
				Decision.Trace.bDegraded
					? *FString::Printf(TEXT("（已降级：%s）"), *Decision.Trace.DegradeReason)
					: TEXT(""),
				Decision.Trace.ElapsedMs,
				Decision.Trace.ViolationCount,
				*USHMDirectorCore::DecisionToString(Decision));
		}));
	}));

// ============================================================================
//  控制台命令：SHM.ExportDecisionLog —— 随时导出当前这局的决策日志
// ============================================================================
static FAutoConsoleCommandWithWorldAndArgs GExportLogCmd(
	TEXT("SHM.ExportDecisionLog"),
	TEXT("导出本局决策日志 JSON 到 Saved/DecisionLogs/"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->GetGameInstance())
		{
			UE_LOG(LogSHMDirectorCore, Warning,
				TEXT("SHM.ExportDecisionLog: 需要在 PIE 运行中执行（先点 Play）"));
			return;
		}
		if (USHMDirectorCore* Core = World->GetGameInstance()->GetSubsystem<USHMDirectorCore>())
		{
			const FString Path = FPaths::ProjectSavedDir() /
				FString::Printf(TEXT("DecisionLogs/Manual_%s.json"),
					*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			Core->ExportDecisionLog(Path);
		}
	}));
