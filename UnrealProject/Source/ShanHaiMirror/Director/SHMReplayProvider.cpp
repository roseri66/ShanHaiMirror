#include "SHMReplayProvider.h"
#include "SHMJsonIntent.h"
#include "SHMDecisionLogFormat.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace SHMLogFormat;

DEFINE_LOG_CATEGORY_STATIC(LogSHMReplay, Log, All);

FSHMReplayProvider::FSHMReplayProvider(const FString& ScriptPath)
{
	const FString Path = ScriptPath.IsEmpty()
		? FPaths::ProjectDir() / TEXT("Data/ReplayScripts/Default.json")
		: ScriptPath;

	bLoaded = LoadScript(Path);
	if (!bLoaded)
	{
		// 不是致命错误：DirectorCore 会改用本地 Provider（失败必须出声，踩坑 #15）
		UE_LOG(LogSHMReplay, Warning, TEXT("回放脚本加载失败，本 Provider 不可用：%s"), *Path);
	}
	else
	{
		UE_LOG(LogSHMReplay, Log, TEXT("回放脚本已加载（%d 层）：%s"), IntentsByFloor.Num(), *Path);
	}
}

bool FSHMReplayProvider::LoadScript(const FString& AbsolutePath)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *AbsolutePath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogSHMReplay, Warning, TEXT("回放脚本不是合法 JSON：%s"), *AbsolutePath);
		return false;
	}

	// 版本检查：格式演进后旧脚本可能语义已变，宁可拒绝也不要静默跑出错误决策
	int32 Version = 0;
	if (!Root->TryGetNumberField(Key_SchemaVersion, Version))
	{
		UE_LOG(LogSHMReplay, Warning, TEXT("回放脚本缺 %s 字段，拒绝加载：%s"), Key_SchemaVersion, *AbsolutePath);
		return false;
	}
	if (Version != SchemaVersion)
	{
		UE_LOG(LogSHMReplay, Warning,
			TEXT("回放脚本版本 %d 与当前格式 %d 不符，拒绝加载：%s"), Version, SchemaVersion, *AbsolutePath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Floors = nullptr;
	if (!Root->TryGetArrayField(Key_Floors, Floors) || !Floors)
	{
		UE_LOG(LogSHMReplay, Warning, TEXT("回放脚本无 %s 数组：%s"), Key_Floors, *AbsolutePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FloorValue : *Floors)
	{
		const TSharedPtr<FJsonObject>* FloorObj = nullptr;
		if (!FloorValue.IsValid() || !FloorValue->TryGetObject(FloorObj) || !FloorObj) { continue; }

		int32 FloorIndex = 0;
		if (!(*FloorObj)->TryGetNumberField(Key_FloorIndex, FloorIndex)) { continue; }

		const TSharedPtr<FJsonObject>* IntentObj = nullptr;
		if (!(*FloorObj)->TryGetObjectField(Key_RawIntent, IntentObj) || !IntentObj) { continue; }

		bool bOk = false;
		const FDirectorIntent Intent = FSHMJsonIntent::ParseFromJsonObject(*IntentObj, bOk);
		if (bOk)
		{
			IntentsByFloor.Add(FloorIndex, Intent);
		}
	}

	return IntentsByFloor.Num() > 0;
}

void FSHMReplayProvider::RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone)
{
	// 脚本缺这一层时 bSuccess=false，让 DirectorCore 降级——回放的价值是确定性，
	// 猜一个凑数等于毁掉它
	const bool bHas = IntentsByFloor.Contains(Context.FloorIndex);
	OnDone.ExecuteIfBound(RequestIntent(Context), bHas);
}

FDirectorIntent FSHMReplayProvider::RequestIntent(const FDirectorContext& Context)
{
	if (const FDirectorIntent* Found = IntentsByFloor.Find(Context.FloorIndex))
	{
		return *Found;
	}

	// 脚本没录到这一层：返回空 Intent（无权重）——护栏会拒，DirectorCore 降级到本地。
	// 比"随便返回上一层的"安全：回放的价值就是确定性，猜等于毁掉它。
	UE_LOG(LogSHMReplay, Warning, TEXT("回放脚本缺第 %d 层，将由上层降级处理"), Context.FloorIndex);
	return FDirectorIntent();
}
