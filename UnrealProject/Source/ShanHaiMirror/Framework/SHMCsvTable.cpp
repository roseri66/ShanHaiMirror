#include "SHMCsvTable.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHMCsvTable, Log, All);

UDataTable* FSHMCsvTable::Load(const FString& AbsoluteFilePath, UScriptStruct* RowStruct, UObject* Outer)
{
	FString CsvContent;
	if (!FFileHelper::LoadFileToString(CsvContent, *AbsoluteFilePath))
	{
		UE_LOG(LogSHMCsvTable, Error, TEXT("无法读取 CSV：%s"), *AbsoluteFilePath);
		return nullptr;
	}

	UDataTable* Table = NewObject<UDataTable>(Outer);
	Table->RowStruct = RowStruct;

	const TArray<FString> Problems = Table->CreateTableFromCSVString(CsvContent);
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogSHMCsvTable, Warning, TEXT("CSV 解析问题（%s）：%s"), *AbsoluteFilePath, *Problem);
	}

	if (Table->GetRowMap().Num() == 0)
	{
		UE_LOG(LogSHMCsvTable, Error, TEXT("CSV 解析后无任何行：%s"), *AbsoluteFilePath);
		return nullptr;
	}

	return Table;
}

UDataTable* FSHMCsvTable::LoadProjectData(const FString& FileName, UScriptStruct* RowStruct, UObject* Outer)
{
	return Load(FPaths::ProjectDir() / TEXT("Data") / FileName, RowStruct, Outer);
}
