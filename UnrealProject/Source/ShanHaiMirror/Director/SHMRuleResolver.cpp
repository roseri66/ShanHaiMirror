#include "SHMRuleResolver.h"
#include "Engine/DataTable.h"
#include "Framework/SHMCsvTable.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHMDirector, Log, All);

FRuleModifier FSHMRuleResolver::Resolve(const FRuleIntent& Intent, const UDataTable* RuleTable)
{
	FRuleModifier Result; // 默认：RuleTag 无效 = 解析失败

	if (!RuleTable)
	{
		UE_LOG(LogSHMDirector, Warning, TEXT("RuleResolver: 规则表为空，无法解析 %s/%s"),
			*Intent.RuleTag.ToString(), *Intent.Level);
		return Result;
	}

	bool bFound = false;
	RuleTable->ForeachRow<FSHMRuleRow>(TEXT("RuleResolver"),
		[&](const FName& RowName, const FSHMRuleRow& Row)
	{
		if (!bFound && Row.RuleTag == Intent.RuleTag && Row.Level == Intent.Level)
		{
			bFound = true;
			Result.RuleTag    = Row.RuleTag;
			Result.Level      = Row.Level;
			Result.Multiplier = Row.Multiplier;   // ★ 数值在全链路第一次出现
			Result.Cost       = Row.Cost;
		}
	});

	if (!bFound)
	{
		// 不崩、不猜数值——查不到就返回无效，让上游丢弃这条规则并留痕。
		// W4 的 LLM 可能编造表里不存在的组合，这里是它落地前的最后一道过滤。
		UE_LOG(LogSHMDirector, Warning, TEXT("RuleResolver: 表中不存在 %s/%s，该规则被丢弃"),
			*Intent.RuleTag.ToString(), *Intent.Level);
	}

	return Result;
}

UDataTable* FSHMRuleResolver::LoadTableFromCsvFile(const FString& AbsoluteFilePath, UObject* Outer)
{
	// 委托给通用 CSV 加载（第三次开工时泛化，与敌人表共用）
	return FSHMCsvTable::Load(AbsoluteFilePath, FSHMRuleRow::StaticStruct(), Outer);
}
