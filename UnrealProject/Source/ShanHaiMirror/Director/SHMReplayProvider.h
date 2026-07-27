#pragma once

#include "CoreMinimal.h"
#include "SHMAIProvider.h"

// ============================================================================
// 回放 Provider —— 从预录脚本按层号取 Intent
//
// 一个抽象吃三个需求里的第三个（D-16）：
//   · 录屏确定性——演示时每次跑出完全一样的决策，不看运气
//   · 集成测试不依赖网络
//   · LLM 解析逻辑的测试台（与 LlmProvider 共用同一套 FSHMJsonIntent）
//
// **脚本格式就是决策日志格式的子集**（SHMDecisionLogFormat.h）：读 floors[].rawIntent。
// 这意味着"打一局真实的 → 导出决策日志 → 直接当回放脚本用"，不需要手写 JSON，
// 也不需要为回放另造一套格式。
// ============================================================================
class SHANHAIMIRROR_API FSHMReplayProvider : public ISHMAIProvider
{
public:
	// ScriptPath 为空则用默认脚本 <项目>/Data/ReplayScripts/Default.json
	explicit FSHMReplayProvider(const FString& ScriptPath = FString());

	// 立即（同步）回调——读内存里的脚本没有等待
	virtual void RequestIntentAsync(const FDirectorContext& Context, FSHMOnIntentReady OnDone) override;
	virtual FString GetProviderName() const override { return TEXT("Replay"); }

	// 同步版（测试直接用）
	FDirectorIntent RequestIntent(const FDirectorContext& Context);

	// 脚本是否成功加载（失败时 DirectorCore 应改用本地 Provider）
	bool IsScriptLoaded() const { return bLoaded; }

	// 脚本里有几层
	int32 GetFloorCount() const { return IntentsByFloor.Num(); }

private:
	bool LoadScript(const FString& AbsolutePath);

	TMap<int32, FDirectorIntent> IntentsByFloor;
	bool bLoaded = false;
};
