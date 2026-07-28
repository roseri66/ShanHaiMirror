#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHMDirectorTypes.h"
#include "SHMAIProvider.h"
// 必须是完整类型而非前向声明：TUniquePtr<FSHMLocalProvider> 的销毁需要它，
// 且 UObject 的 .gen.cpp 也会实例化析构（那里加不了 include）
#include "SHMLocalProvider.h"
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

	// 决策就绪回调
	DECLARE_DELEGATE_OneParam(FSHMOnDecisionReady, const FDirectorDecision& /*Decision*/);

	// 主入口（异步）：建上下文 → Provider 取 Intent → 四护栏 → 查表出数值 → 回调。
	// 本地/回放 Provider 会在调用内立即回调；LLM 在响应返回时回调。
	// **任何一级失败都降级，回调必定被调用一次**——调用方不需要处理"没回调"的情况。
	void DecideForFloorAsync(const FPlayerProfile& Profile, int32 FloorIndex, FSHMOnDecisionReady OnDecision);

	// 同步入口：强制走本地 Provider（控制台调试/单测用，不发网络）
	FDirectorDecision DecideForFloor(const FPlayerProfile& Profile, int32 FloorIndex);

	// Run 重开时清空决策历史
	UFUNCTION(BlueprintCallable, Category = "AI Director")
	void ResetRun();

	// --- 查询（调试/日志/时间轴用）---
	const TArray<FDirectorHistoryEntry>& GetDecisionHistory() const { return DecisionHistory; }

	// 整局逐层留痕：时间轴 UI 与统计命令的数据源
	const TArray<FSHMFloorRecord>& GetFloorRecords() const { return FloorRecords; }

	// 当前生效规则的数值查询——玩法作用面（伤害/冷却计算）从这里取倍率。
	// 未命中返回 1.0（无修改）。这是玩法层消费 FDirectorDecision 的便捷入口，
	// 不引入新接口：读的仍是最近一次 DecideForFloor 产出的决策。
	UFUNCTION(BlueprintPure, Category = "AI Director")
	float GetActiveRuleMultiplier(FGameplayTag RuleTag) const;

	// 挑战预算曲线：F0 = 0（只观察，对齐 GDD「F1 建立画像不调整」），随层深递增
	static int32 ChallengeBudgetForFloor(int32 FloorIndex);

	// 决策转可读文本（DumpDecision 与 W5 报告 UI 共用）
	static FString DecisionToString(const FDirectorDecision& Decision);

	// 决策日志：整局的「看到什么→判断什么→改了什么」，可导出 JSON
	// （P3 格式，回放 Provider 与后续可视化共用）
	UFUNCTION(BlueprintCallable, Category = "AI Director")
	bool ExportDecisionLog(const FString& AbsolutePath) const;

	// 首层观察决策（代码强制，不进 Provider）。
	// 公开是因为 FloorManager 也用它做异步回调到达前的垫底值。
	static FDirectorDecision MakeObserveFloorDecision();

	// 配置选中的 Provider 名（Local / Llm / Replay）
	UFUNCTION(BlueprintPure, Category = "AI Director")
	FString GetProviderName() const { return Provider ? Provider->GetProviderName() : TEXT("None"); }

	// **实际决策者**的可读描述——UI 该显示这个而不是上面那个。
	// 配置成 Llm 但每次都降级时，玩家看到的应该是「Llm → 已降级本地」，
	// 而不是一个误导性的「Llm」。没有决策记录时回落到配置值。
	UFUNCTION(BlueprintPure, Category = "AI Director")
	FString GetEffectiveSourceLabel() const;

	// AI 导演总开关（控制台 SHM.Director 0/1）。
	// 关闭 = 固定均衡配比、零规则、白泽不出声，游戏退化为普通固定难度刷怪 Roguelike。
	// **这是"抽掉 AI 体验就坍塌"的证明方式**（DECISIONS §4.7 红线项）：
	// 不是让游戏坏掉，而是让它变得平庸——两局对照才有说服力。
	static bool IsDirectorEnabled();

	// 关闭态下的固定决策（不读画像、不调 Provider、不出台词）
	static FDirectorDecision MakeDirectorOffDecision();

private:
	FDirectorContext BuildContext(const FPlayerProfile& Profile, int32 FloorIndex) const;

	// Intent 就绪后的公共收尾：护栏 → 查表 → 记历史/日志 → 回调
	void FinishDecision(const FDirectorContext& Context, const FDirectorIntent& Intent,
	                    const FString& ProviderId, bool bDegraded, const FString& DegradeReason,
	                    float ElapsedMs, FSHMOnDecisionReady OnDecision);

	// 校验失败时的安全兜底：全杂兵、零规则——宁可保守，不可违规
	FDirectorDecision MakeSafeFallbackDecision(const FString& Reason) const;

	// 记一层决策进日志（含护栏前 RawIntent 与护栏判定，事后无法补造）
	void RecordLogEntry(const FDirectorContext& Context, const FDirectorIntent& RawIntent,
	                    const FValidationResult& Validation, const FDirectorDecision& Decision);

	UPROPERTY()
	TObjectPtr<UDataTable> RuleTable;

	TUniquePtr<ISHMAIProvider> Provider;

	// 降级终点：永远可用，不参与 Provider 选择（Provider 失败时直接调它）
	TUniquePtr<FSHMLocalProvider> LocalFallback;

	TArray<FDirectorHistoryEntry> DecisionHistory;

	// 最近一次产出的决策（含观察层与安全兜底），规则倍率查询的数据源
	FDirectorDecision LastDecision;

	// 整局决策日志（JSON 对象数组，导出时套上顶层信封）
	TArray<TSharedPtr<class FJsonObject>> LogEntries;

	// 同一批数据的运行时形态（时间轴/统计用，免得为了画界面去反解析 JSON）
	UPROPERTY()
	TArray<FSHMFloorRecord> FloorRecords;
	FString RunId;
	FString RunStartedAt;
};
