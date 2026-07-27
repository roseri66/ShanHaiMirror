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

	// 弹出报告卡（层间调用）。Duration 秒后自动淡出
	void ShowDirectorReport(const FPlayerProfile& Profile, const FDirectorDecision& Decision,
	                        int32 FloorIndex, float Duration = 6.f);

	// 立即收起（玩家跳过 / 进入战斗）
	void HideDirectorReport();

private:
	void DrawReportCard(float Alpha);
	void DrawDirectorStatusBadge();

	// 一行文本，返回本行占用的高度（供调用方累加 Y）
	float DrawLine(const FString& Text, float X, float Y, const FLinearColor& Color,
	               float Scale = 1.f) const;

	bool  bShowingReport = false;
	float ReportShownAt  = 0.f;
	float ReportDuration = 6.f;

	FPlayerProfile    CachedProfile;
	FDirectorDecision CachedDecision;
	int32             CachedFloorIndex = 0;
};
