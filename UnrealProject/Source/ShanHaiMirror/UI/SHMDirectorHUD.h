#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Framework/SHMCoreTypes.h"
#include "SHMDirectorHUD.generated.h"

// ============================================================================
// 导演报告 HUD —— 层间弹出的「白泽的观察」报告卡
//
// 为什么用 AHUD::DrawHUD 而不是 UMG：**零编辑器资产**。
// UMG 要建 WBP、连绑定、配 GameMode，全是手工点击；本项目没有美术资源
// （D-06），报告卡要的是「信息层次清楚、能截图、能录屏」，不是视觉效果。
// 纯 C++ 绘制让整块 UI 可以随代码走 git、随构建验证，不依赖任何人去点编辑器。
//
// 它同时解决两个问题：
//   ① 决策不可见——前四次开工做的是「系统正确」，这里让正确变得可见
//   ② LLM 延迟空白——DeepSeek 单次 3.8~5.0s，层间过场因此拉长到 4~6 秒。
//      玩家读报告的时间自然吸收掉延迟（TDD §1.2 的原设计意图）
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API ASHMDirectorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// 弹出报告卡（层间调用）
	void ShowDirectorReport(const FPlayerProfile& Profile, const FDirectorDecision& Decision,
	                        int32 FloorIndex);

	// 立即收起
	void HideDirectorReport();

	// --- 供 FloorManager 判断「能不能开打了」---
	bool IsReportShowing() const { return bShowingReport; }

	// 玩家是否已经看够了：过了最短阅读时间且（按了继续键 或 到了自动关闭时间）
	bool IsReportAcknowledged() const;

	// 卡片已显示多久
	float GetReportElapsed() const;

	// 最短阅读时间——防手滑瞬间跳过（也保证录屏里卡片一定看得见）
	static constexpr float MinReadSeconds  = 1.2f;
	// 无操作时自动继续
	static constexpr float AutoDismissSeconds = 7.f;

	// --- 镜界时间轴：整局「看到什么 → 想改什么 → 护栏拦没拦 → 实际改了什么」---
	void ToggleTimeline() { bShowingTimeline = !bShowingTimeline; }
	void SetTimelineVisible(bool bVisible) { bShowingTimeline = bVisible; }
	bool IsTimelineShowing() const { return bShowingTimeline; }

private:
	// 一行待绘制的文本
	struct FReportLine
	{
		FString      Text;
		FLinearColor Color;
		float        Scale   = 1.f;
		float        PadBelow = 0.f;
	};

	// 先把内容排成行表，再据此算卡片高度——**边框高度必须由内容算出**，
	// 写死会在规则条数变化/出现降级行时溢出（实测踩过）
	void BuildReportLines(TArray<FReportLine>& Out) const;

	void DrawReportCard(float Alpha);
	void DrawDirectorStatusBadge();
	void DrawTimeline();

	float DrawLine(const FString& Text, float X, float Y, const FLinearColor& Color,
	               float Scale = 1.f) const;

	bool  bShowingReport   = false;
	bool  bShowingTimeline = false;
	float ReportShownAt    = 0.f;

	FPlayerProfile    CachedProfile;
	FDirectorDecision CachedDecision;
	int32             CachedFloorIndex = 0;
};
