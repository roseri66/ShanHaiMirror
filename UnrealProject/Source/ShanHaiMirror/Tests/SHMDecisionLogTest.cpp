#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Director/SHMDirectorCore.h"
#include "Director/SHMDecisionLogFormat.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// 决策日志导出测试
//
// 守的是 P3 的承诺：这份 JSON 有三个消费方（回放 Provider / 前端可视化 /
// 可能的后端），任何一方读不了都算契约破了。**编码也是契约的一部分**——
// 实测踩过：UE 的 SaveStringToFile 遇到非 ASCII（中文台词）默认写 UTF-16 LE，
// UE 自己读没事，但 JSON 交换标准是 UTF-8，网页端和 Python 都会直接失败。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags LogTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMDecisionLogEncodingTest,
	"SHM.Director.DecisionLog.Export_IsUtf8AndParsable", LogTestFlags)
bool FSHMDecisionLogEncodingTest::RunTest(const FString& Parameters)
{
	// GameInstanceSubsystem 的 ClassWithin 是 UGameInstance，不能挂在 transient package
	// 下（会触发 ensure）。造一个临时 GameInstance 当 Outer 即可——
	// 本测试不跑游戏流程，只验导出这一个纯数据动作。
	UGameInstance* TempGI = NewObject<UGameInstance>(GEngine);
	USHMDirectorCore* Core = NewObject<USHMDirectorCore>(TempGI);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("TestLogs/EncodingCheck.json");

	if (!TestTrue(TEXT("导出应成功"), Core->ExportDecisionLog(Path)))
	{
		return false;
	}

	// 按**字节**读，才能看出真实编码——按字符串读会被自动转码掩盖问题
	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("导出的文件应可读"), FFileHelper::LoadFileToArray(Bytes, *Path)))
	{
		return false;
	}
	if (!TestTrue(TEXT("文件不应为空"), Bytes.Num() > 8))
	{
		return false;
	}

	// UTF-16 LE BOM = FF FE，UTF-16 BE BOM = FE FF，UTF-8 BOM = EF BB BF
	const bool bUtf16 = (Bytes[0] == 0xFF && Bytes[1] == 0xFE) || (Bytes[0] == 0xFE && Bytes[1] == 0xFF);
	TestFalse(TEXT("不得是 UTF-16（网页端/Python 无法直接解析）"), bUtf16);

	const bool bUtf8Bom = (Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF);
	TestFalse(TEXT("不应带 UTF-8 BOM（部分 JSON 解析器不认）"), bUtf8Bom);

	// 首字符应直接是 '{'——这是「能被任意 JSON 解析器直读」的最直接判据
	TestEqual(TEXT("首字节应为 '{'"), static_cast<int32>(Bytes[0]), static_cast<int32>('{'));

	// schemaVersion 必须在，且是格式约定的版本
	FString Content;
	FFileHelper::LoadFileToString(Content, *Path);
	TestTrue(TEXT("应含 schemaVersion 字段"), Content.Contains(SHMLogFormat::Key_SchemaVersion));
	TestTrue(TEXT("应含 floors 数组"),        Content.Contains(SHMLogFormat::Key_Floors));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
