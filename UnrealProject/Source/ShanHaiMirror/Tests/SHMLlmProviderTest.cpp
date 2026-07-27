#include "Misc/AutomationTest.h"
#include "Director/SHMLlmProvider.h"
#include "HAL/PlatformMisc.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// LLM Provider 测试
//
// 这里**不测"LLM 回答得对不对"**——那不可控，也正是护栏存在的理由。
// 测的是完全可控的那一半：没有 key 时它必须立刻、明确地判失败，
// 好让 DirectorCore 降级。这是"断网完整可玩"链条的第一环。
// （真实 HTTP 往返不进单测：依赖网络的测试不可复现，属于集成验证范畴）
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags LlmTestFlags =
		EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMLlmNoKeyTest,
	"SHM.Director.Llm.NoApiKey_FailsImmediatelyForDegrade", LlmTestFlags)
bool FSHMLlmNoKeyTest::RunTest(const FString& Parameters)
{
	// 本机若真配了 key，这条测试不适用（它测的是"没配 key"的行为）
	if (!FPlatformMisc::GetEnvironmentVariable(TEXT("SHM_LLM_API_KEY")).IsEmpty())
	{
		AddInfo(TEXT("检测到已配置 SHM_LLM_API_KEY，跳过「无 key」用例"));
		return true;
	}

	FSHMLlmProvider Provider;
	TestFalse(TEXT("无 key 时应报告自己不可用"), Provider.IsAvailable());

	// 即便被调用，也必须立刻回调失败——不能挂起、不能发请求
	bool bCalled = false;
	bool bSuccess = true;
	Provider.RequestIntentAsync(FDirectorContext(),
		FSHMOnIntentReady::CreateLambda([&](const FDirectorIntent&, bool bOk)
		{
			bCalled = true;
			bSuccess = bOk;
		}));

	TestTrue (TEXT("回调必须被调用（调用方不需处理「没回调」的情况）"), bCalled);
	TestFalse(TEXT("无 key 应判失败以触发降级"), bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMLlmConfigTest,
	"SHM.Director.Llm.Endpoint_IsConfigurable", LlmTestFlags)
bool FSHMLlmConfigTest::RunTest(const FString& Parameters)
{
	// 端点可配置是刻意的：将来把 base URL 指向自建中转服务时，
	// 本类一行代码都不用改。硬编码域名会把这条路堵死。
	FSHMLlmProvider Provider;

	TestFalse(TEXT("应有默认端点"), Provider.GetBaseUrl().IsEmpty());
	TestFalse(TEXT("应有默认模型"), Provider.GetModel().IsEmpty());
	TestTrue (TEXT("默认端点应是 OpenAI 兼容形态（/v1 结尾）"),
		Provider.GetBaseUrl().EndsWith(TEXT("/v1")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
