#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHMDirectorTypes.h"
#include "SHMAIProvider.h"
#include "SHMDirectorCore.generated.h"

class UDataTable;

// ============================================================================
// 导演核心 —— 编排链路 ③ CONSTRAIN → ④ CHOOSE → ⑤ VALIDATE → ⑥ RESOLVE
//
// **明确不做**：不直接改游戏对象（FloorGenerator 在 W3 消费决策）、
// 不发 HTTP（Provider 的事）、不算画像（Analyzer 的事）。
//
// 类型说明：DECISIONS §4.1 原写 UWorldSubsystem，此处改为 UGameInstanceSubsystem——
// 一层跨多个房间会有 Level Streaming/切图，World 级生命周期会丢掉决策历史；
// 决策历史既是 Fairness 护栏的输入，也是 W5 决策日志的数据源，必须活过整个 Run。
// （DECISIONS 已同步补充修正说明。）
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API USHMDirectorCore : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 主入口：给定画像与层号，产出玩法层可直接消费的决策。
	// 内部完成：建上下文 → Provider 选择 → 四护栏校验 → 查表出数值 → 记历史。
	FDirectorDecision DecideForFloor(const FPlayerProfile& Profile, int32 FloorIndex);

	// Run 重开时清空决策历史
	UFUNCTION(BlueprintCallable, Category = "AI Director")
	void ResetRun();

	// --- 查询（调试/W5 日志用）---
	const TArray<FDirectorHistoryEntry>& GetDecisionHistory() const { return DecisionHistory; }

	// 挑战预算曲线：F0 = 0（只观察，对齐 GDD「F1 建立画像不调整」），随层深递增
	static int32 ChallengeBudgetForFloor(int32 FloorIndex);

	// 决策转可读文本（DumpDecision 与 W5 报告 UI 共用）
	static FString DecisionToString(const FDirectorDecision& Decision);

private:
	FDirectorContext BuildContext(const FPlayerProfile& Profile, int32 FloorIndex) const;

	// 校验失败时的安全兜底：全杂兵、零规则——宁可保守，不可违规
	FDirectorDecision MakeSafeFallbackDecision(const FString& Reason) const;

	UPROPERTY()
	TObjectPtr<UDataTable> RuleTable;

	TUniquePtr<ISHMAIProvider> Provider;

	TArray<FDirectorHistoryEntry> DecisionHistory;
};
