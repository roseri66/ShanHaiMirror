#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

class UDataTable;

// ============================================================================
// 规则解析器 —— AI Director 链路第 ⑥ 步 RESOLVE
//
// **数值只在这里产生。** (Rule.Ammo, medium) → 查 DT_Rule → ×0.70。
// Provider（含未来 LLM）的输出里没有数值，玩法层拿到的已是带数值的成品，
// 两边都不需要、也不允许自己算数值。
//
// 为可测性：表以参数传入，本类不 LoadObject、不持有状态。
// ============================================================================
class SHANHAIMIRROR_API FSHMRuleResolver
{
public:
	// (标签, 等级) → 带数值的规则修改器。查不到时返回 RuleTag 无效的修改器并打日志。
	static FRuleModifier Resolve(const FRuleIntent& Intent, const UDataTable* RuleTable);

	// 从 CSV 文件构建规则表（运行时/测试通用；文件缺失或解析失败返回 nullptr 并打日志）
	static UDataTable* LoadTableFromCsvFile(const FString& AbsoluteFilePath, UObject* Outer);
};
