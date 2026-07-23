#include "SHMDirectorCore.h"
#include "SHMLocalProvider.h"
#include "SHMRuleResolver.h"
#include "SHMDecisionValidator.h"
#include "Framework/SHMGameplayTags.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"

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

	// W2 只有本地 Provider；W4 起按配置切换 Llm/Replay，DirectorCore 代码不变
	Provider = MakeUnique<FSHMLocalProvider>();

	ResetRun();
}

void USHMDirectorCore::ResetRun()
{
	DecisionHistory.Empty();
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

FDirectorDecision USHMDirectorCore::DecideForFloor(const FPlayerProfile& Profile, int32 FloorIndex)
{
	if (!Provider)
	{
		return MakeSafeFallbackDecision(TEXT("Provider 未初始化"));
	}

	// ③ CONSTRAIN
	const FDirectorContext Ctx = BuildContext(Profile, FloorIndex);

	// ④ CHOOSE
	const FDirectorIntent Intent = Provider->RequestIntent(Ctx);

	// ⑤ VALIDATE —— 本地 Provider 理论上必过；不过即为 Provider bug，日志揭露
	const FValidationResult Validation = FSHMDecisionValidator::Validate(Intent, Ctx);
	if (!Validation.bValid)
	{
		for (const FString& Reason : Validation.RejectReasons)
		{
			UE_LOG(LogSHMDirectorCore, Error, TEXT("[%s] 决策被护栏拒绝：%s"),
				*Provider->GetProviderName(), *Reason);
		}
		return MakeSafeFallbackDecision(TEXT("护栏拒绝，降级安全决策"));
	}

	// ⑥ RESOLVE —— 数值在此产生
	FDirectorDecision Decision;
	Decision.ChallengeLevel = Intent.ChallengeLevel;
	Decision.EnemyWeights   = Intent.EnemyWeights;
	Decision.NarrationLine  = Intent.Narration;
	Decision.Reason         = Intent.Reason;

	for (const FRuleIntent& RuleIntent : Intent.RuleIntents)
	{
		const FRuleModifier Resolved = FSHMRuleResolver::Resolve(RuleIntent, RuleTable);
		if (Resolved.RuleTag.IsValid())
		{
			Decision.RuleModifiers.Add(Resolved);
		}
		// 解析失败的规则被静默丢弃（Resolver 已打日志）——决策仍然可用，只是少一条规则
	}

	// 记历史：Fairness 护栏与 W5 决策日志的数据源
	FDirectorHistoryEntry Entry;
	Entry.FloorIndex = FloorIndex;
	for (const FRuleModifier& Mod : Decision.RuleModifiers)
	{
		Entry.AppliedRuleTags.Add(Mod.RuleTag);
	}
	DecisionHistory.Add(Entry);

	UE_LOG(LogSHMDirectorCore, Log, TEXT("F%d 决策完成：\n%s"), FloorIndex, *DecisionToString(Decision));
	return Decision;
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
		if (!World || !World->GetGameInstance()) { return; }
		USHMDirectorCore* Core = World->GetGameInstance()->GetSubsystem<USHMDirectorCore>();
		if (!Core) { return; }

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

		const FDirectorDecision Decision = Core->DecideForFloor(Profile, Floor);
		UE_LOG(LogSHMDirectorCore, Display, TEXT("=== SHM.DumpDecision (%s, F%d) ===\n%s"),
			bVanguard ? TEXT("vanguard") : TEXT("ranger"), Floor,
			*USHMDirectorCore::DecisionToString(Decision));
	}));
