#pragma once

#include "CoreMinimal.h"
#include "SHMAIProvider.h"
#include "Interfaces/IHttpRequest.h"

// ============================================================================
// Remote Provider —— 决策网关后端（DirectorService）的客户端（D-23）
//
// 与 FSHMLlmProvider 的区别不在"都是 HTTP"，在**它调的是自己的服务**：
//   · key 留在服务端，客户端不再持有任何凭据 —— 这是 D-23 存在的第一条理由
//   · prompt 真源在服务端，客户端不再拼 prompt
//   · 响应体**就是 Intent 本体，不带信封**，故直接复用 FSHMJsonIntent::ParseFromJson
//     ——那个"不信任 LLM 输出的第一道关卡"原封不动变成"不信任后端输出"，一行不改
//
// **它是可失败的，而且失败被视为正常路径**：后端没起、超时、5xx、429 限流、
// 200 但 body 是垃圾，一律回调 bSuccess=false，由 DirectorCore 降级到本地表。
// 不变量②「断网可玩」在本 Provider 上多了一个新的失败模式：
// 以前"有网"就意味着"能到 LLM"，现在中间多了一跳——后端进程停掉而网络正常，
// 是本次新增的回归用例（见 DECISIONS D-23）。
//
// 护栏仍在客户端。本 Provider 的输出和其它三个一样要过四道护栏，
// 不因为"是自己的服务"就给免检特权（D-23 明确否决了护栏上服务端）。
//
// 配置（全部走环境变量）：
//   SHM_DIRECTOR_URL      选填，默认 http://localhost:8080
//                         为空即用默认；显式设为 off/disabled 可关掉本 Provider
//   SHM_DIRECTOR_TIMEOUT  选填，秒，默认 12
// ============================================================================
class SHANHAIMIRROR_API FSHMRemoteProvider : public ISHMAIProvider
{
public:
	// 生产构造：从环境变量读配置
	FSHMRemoteProvider();

	// 测试构造：显式给配置，**不读环境变量**。
	// 自动化测试不该受本机配置影响——本机有没有设 SHM_DIRECTOR_URL，
	// 测试结果都必须一样。与 USHMDirectorCore::SetupForTesting() 同一个理由。
	struct FTestingConfig
	{
		FString BaseUrl;
		float   TimeoutSeconds = DefaultTimeoutSeconds;
	};
	explicit FSHMRemoteProvider(const FTestingConfig& Config);

	virtual void RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone) override;
	virtual FString GetProviderName() const override { return TEXT("Remote"); }
	virtual void OnRunStarted(const FString& InRunId) override { RunId = InRunId; }
	virtual FString GetLastFailureReason() const override { return LastFailureReason; }

	// 是否可用。URL 被显式关掉时返回 false，DirectorCore 直接跳过本 Provider。
	//
	// ⚠️ 刻意**不**在这里探测后端是否真的活着：那需要一次同步网络往返，
	// 会卡住游戏线程；而且"启动时活着"不代表"决策时还活着"。
	// 后端不可达由每次请求各自失败并降级处理 —— 这正是降级链存在的意义。
	// 这与 FSHMLlmProvider 的 IsAvailable() 语义一致：它查的也是"配了没有"，
	// 不是"能不能通"。
	bool IsAvailable() const { return !BaseUrl.IsEmpty(); }

	const FString& GetBaseUrl() const { return BaseUrl; }
	float GetTimeoutSeconds()   const { return TimeoutSeconds; }

	// 默认 12 秒：必须大于服务端自己的上游超时（服务端调 LLM 约 10s），
	// 否则客户端会先于服务端放弃，服务端那次调用就成了纯浪费。
	// 同时必须小于 USHMFloorManager::MaxDecisionWaitSeconds，
	// 否则玩法层会先放弃，等待成空耗。三者的大小关系是：
	//   服务端上游超时 < 本超时 < 玩法层最大等待
	static constexpr float DefaultTimeoutSeconds = 12.f;

	// 上行路径。与服务端的 @PostMapping 必须一致——
	// 这是跨语言契约的一部分，改这里要同步改 DirectorService。
	static const TCHAR* IntentPath;

private:
	// 存活令牌 —— 防 use-after-free。
	//
	// 与 FSHMLlmProvider 同一个坑、同一套解法（2026-07-29 代码审查发现的那条）：
	// HTTP 往返期间玩家停掉 PIE，GameInstance 连同 DirectorCore、连同本 Provider
	// 一起析构，而请求仍在飞行，响应到达时回调仍会执行。
	// 本类不是 UObject，用不了 CreateWeakLambda，故用共享指针当令牌：
	// lambda 只捕获弱引用，pin 不住就说明 Provider 没了，直接返回。
	//
	// **以后在这个项目里加任何异步回调，都要先想生命周期。**
	TSharedPtr<uint8> LifetimeToken = MakeShared<uint8>(0);

	// 启动时探测一次可达性，**只为了打一条明确的提示日志**。
	//
	// ⚠️ 它**不参与 IsAvailable() 判断**，这与头文件上方那条
	// 「刻意不探测连通性」不矛盾：那条说的是"可用性判断不该依赖探测"
	// （探测要同步往返会卡住游戏线程，且启动时活着不代表决策时还活着）。
	// 这里是异步的、纯提示性的，失败也不改变任何行为。
	//
	// 为什么值得加：2026-08-05 用户反复报"连不上服务端一直降级"，
	// 排查一轮后发现只是**服务没启动**。日志里其实写了"后端不可达"，
	// 但那是每层决策时才出现的 Warning，而且和"超时"长得一样。
	// 开局就明说「网关不可达，本局全程本地」并给出启动命令，
	// 比让人事后翻日志强得多。踩坑 #16「功能没长嘴」的同一课。
	void ProbeReachability();

	FString BaseUrl;
	FString RunId;
	float   TimeoutSeconds = DefaultTimeoutSeconds;

	// 最近一次失败的具体原因，供报告卡角标与决策日志区分
	// 「不可达」「超时」「被限流」「响应无法解析」。
	// 只有一个线程写（HTTP 回调都在游戏线程派发），不需要额外同步。
	FString LastFailureReason;
};
