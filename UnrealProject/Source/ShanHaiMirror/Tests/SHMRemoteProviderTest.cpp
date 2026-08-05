#include "Misc/AutomationTest.h"
#include "Director/SHMRemoteProvider.h"
#include "Director/SHMLocalProvider.h"
#include "Director/SHMJsonIntent.h"

// ============================================================================
// Remote Provider（D-23）的可测部分
//
// **能测什么、不能测什么，先说清楚：**
//
// 可测（本文件）：配置语义、Provider 标识、禁用时的即时失败、上行路径常量、
//   以及三者的超时大小关系（服务端上游 < 本 Provider < 玩法层最大等待）。
//
// 不可测（需真实 HTTP，属人工/集成验收）：后端不可达、超时、5xx、429。
//   这三条是 DECISIONS D-23 要求每个里程碑跑一遍的降级回归，靠真实起停后端验。
//
// 已被别处覆盖、不在这里重复：
//   · 「200 但 body 是垃圾」——Remote 直接复用 FSHMJsonIntent::ParseFromJson，
//     而 JsonIntent.Malformed_FailsSafely / MissingFields_UsesSafeDefaults
//     已经把那个解析器的失败路径测透了。复用同一个函数，覆盖自然传递，
//     再写一遍只是把同一件事测两次。
//   · 「Provider 交不出结果时 DirectorCore 怎么办」——Degrade.ProviderFails_*
//     用测试替身测过，与具体是哪个 Provider 无关。
//
// 命名带 Remote 前缀：unity build 会合并匿名命名空间（踩坑 #25）。
// ============================================================================

namespace
{
	constexpr EAutomationTestFlags RemoteTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	FSHMRemoteProvider RemoteMakeProvider(const FString& Url)
	{
		FSHMRemoteProvider::FTestingConfig Cfg;
		Cfg.BaseUrl = Url;
		return FSHMRemoteProvider(Cfg);
	}
}

// ---------------------------------------------------------------------------
// 标识：进 FDirectorTrace.ProviderId，也是报告卡角标显示的来源
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteNameTest,
	"SHM.Director.Remote.ProviderName_IsRemote", RemoteTestFlags)

bool FRemoteNameTest::RunTest(const FString&)
{
	FSHMRemoteProvider P = RemoteMakeProvider(TEXT("http://localhost:8080"));
	TestEqual(TEXT("Provider 标识应为 Remote（报告卡与决策日志都用它）"),
		P.GetProviderName(), FString(TEXT("Remote")));
	return true;
}

// ---------------------------------------------------------------------------
// 可用性：只看"配了没有"，不探测"通不通"
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteAvailabilityTest,
	"SHM.Director.Remote.IsAvailable_ReflectsConfigNotReachability", RemoteTestFlags)

bool FRemoteAvailabilityTest::RunTest(const FString&)
{
	FSHMRemoteProvider Configured = RemoteMakeProvider(TEXT("http://localhost:8080"));
	TestTrue(TEXT("配了 URL 就算可用"), Configured.IsAvailable());

	// 关键：**一个根本不存在的地址也应报"可用"**。
	// IsAvailable() 查的是配置，不是连通性——探测连通性要一次同步往返，
	// 会卡住游戏线程，而且"启动时活着"不代表"决策时还活着"。
	// 后端不可达由每次请求各自失败并降级处理。
	FSHMRemoteProvider Unreachable = RemoteMakeProvider(TEXT("http://127.0.0.1:59999"));
	TestTrue(TEXT("不可达的地址仍算「已配置」——连通性不在这一层判断"),
		Unreachable.IsAvailable());

	FSHMRemoteProvider Disabled = RemoteMakeProvider(FString());
	TestFalse(TEXT("空 URL = 已禁用，DirectorCore 应跳过本 Provider"),
		Disabled.IsAvailable());
	return true;
}

// ---------------------------------------------------------------------------
// 禁用时必须**立即**回调失败，且恰好一次
//
// 「回调必定被调用一次」是 ISHMAIProvider 的契约（Degrade.Callback_FiresExactlyOnce
// 守的就是它）。禁用路径是唯一一条不发 HTTP 的路径，最容易漏掉回调——
// 漏了的话 DirectorCore 会永远等下去，表现为层间过场卡死。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteDisabledFailsFastTest,
	"SHM.Director.Remote.Disabled_FailsImmediatelyExactlyOnce", RemoteTestFlags)

bool FRemoteDisabledFailsFastTest::RunTest(const FString&)
{
	FSHMRemoteProvider Disabled = RemoteMakeProvider(FString());

	int32 CallCount = 0;
	bool  bReportedSuccess = true;

	FSHMOnIntentReady OnDone;
	OnDone.BindLambda([&CallCount, &bReportedSuccess](const FDirectorIntent&, bool bSuccess)
	{
		++CallCount;
		bReportedSuccess = bSuccess;
	});

	FDirectorContext Ctx;
	Disabled.RequestIntentAsync(Ctx, OnDone);

	// 同步完成——没有 HTTP 往返，不需要 tick 消息循环
	TestEqual(TEXT("回调必须恰好触发一次（漏了会让层间过场永远等下去）"), CallCount, 1);
	TestFalse(TEXT("禁用时必须报失败，由 DirectorCore 降级本地"), bReportedSuccess);
	return true;
}

// ---------------------------------------------------------------------------
// 失败原因必须可区分
//
// 2026-08-05：用户反复报"连不上服务端一直降级"，排查一轮后发现只是
// **服务没启动**。日志里写了"后端不可达"，但它和"超时"在报告卡上
// 长得一模一样——分不清为什么降级时，第一反应都是"是不是代码坏了"。
// 降级本身不是问题，**分不清原因才是**。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteFailureReasonTest,
	"SHM.Director.Remote.FailureReason_IsDistinguishable", RemoteTestFlags)

bool FRemoteFailureReasonTest::RunTest(const FString&)
{
	FSHMRemoteProvider Disabled = RemoteMakeProvider(FString());

	// 初始状态不该带上一次的残留
	TestTrue(TEXT("初始时不应有失败原因"), Disabled.GetLastFailureReason().IsEmpty());

	FSHMOnIntentReady OnDone;
	OnDone.BindLambda([](const FDirectorIntent&, bool) {});
	FDirectorContext Ctx;
	Disabled.RequestIntentAsync(Ctx, OnDone);

	TestEqual(TEXT("禁用时原因应为「已禁用」，而非笼统的失败"),
		Disabled.GetLastFailureReason(), FString(TEXT("已禁用")));

	// 接口默认实现返回空——现有三个 Provider 不实现它也不会出错
	FSHMLocalProvider Local;
	TestTrue(TEXT("未实现该方法的 Provider 应返回空，调用方回退通用措辞"),
		Local.GetLastFailureReason().IsEmpty());
	return true;
}

// ---------------------------------------------------------------------------
// 超时的大小关系 —— 这是三个数之间的约束，不是一个孤立的常数
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemoteTimeoutOrderingTest,
	"SHM.Director.Remote.Timeout_LeavesRoomForServerUpstream", RemoteTestFlags)

bool FRemoteTimeoutOrderingTest::RunTest(const FString&)
{
	// 服务端调 LLM 约 10s（SHM_LLM_TIMEOUT 的实测值，见 README）。
	// 客户端必须给服务端留出余量，否则客户端先放弃，
	// 服务端那次上游调用就成了纯浪费——花了钱没人要结果。
	constexpr float ServerUpstreamTimeout = 10.f;

	TestTrue(TEXT("Remote 超时必须大于服务端上游超时，否则服务端白调一次 LLM"),
		FSHMRemoteProvider::DefaultTimeoutSeconds > ServerUpstreamTimeout);

	FSHMRemoteProvider P = RemoteMakeProvider(TEXT("http://localhost:8080"));
	TestEqual(TEXT("未配置环境变量时用默认超时"),
		P.GetTimeoutSeconds(), FSHMRemoteProvider::DefaultTimeoutSeconds);
	return true;
}

// ---------------------------------------------------------------------------
// 上行路径是跨语言契约的一部分
//
// 这个字符串必须与 DirectorService 的 @PostMapping 一致。改一边不改另一边，
// 表现是 404 → 每层降级本地 → 玩家零感知，**问题会被降级链悄悄吞掉**。
// 钉在测试里，至少改动时会有人看见。
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRemotePathContractTest,
	"SHM.Director.Remote.IntentPath_MatchesServerContract", RemoteTestFlags)

bool FRemotePathContractTest::RunTest(const FString&)
{
	TestEqual(TEXT("上行路径必须与 DirectorService 的 @PostMapping 一致"),
		FString(FSHMRemoteProvider::IntentPath), FString(TEXT("v1/director/intent")));
	return true;
}
