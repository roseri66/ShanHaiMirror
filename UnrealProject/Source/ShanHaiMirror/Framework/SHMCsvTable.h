#pragma once

#include "CoreMinimal.h"

class UDataTable;
class UScriptStruct;

// ============================================================================
// CSV → DataTable 通用加载 —— 规则表 / 敌人表共用
// 数据文件放 <项目>/Data/（不在 Content/，见踩坑 #13）
// ============================================================================
class SHANHAIMIRROR_API FSHMCsvTable
{
public:
	// 失败返回 nullptr 并打日志（文件缺失 / 解析失败 / 零行）
	static UDataTable* Load(const FString& AbsoluteFilePath, UScriptStruct* RowStruct, UObject* Outer);

	// 便捷：<项目>/Data/ 下的相对文件名
	static UDataTable* LoadProjectData(const FString& FileName, UScriptStruct* RowStruct, UObject* Outer);
};
