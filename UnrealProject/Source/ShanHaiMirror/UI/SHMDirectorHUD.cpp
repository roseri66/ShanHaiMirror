#include "SHMDirectorHUD.h"
#include "Director/SHMDirectorCore.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
	// 卡片配色：暗底 + 青色主调（对齐「镜界」的冷色调，也保证白字可读）
	const FLinearColor CardBg      (0.03f, 0.05f, 0.08f, 0.88f);
	const FLinearColor CardBorder  (0.35f, 0.75f, 0.85f, 1.f);
	const FLinearColor TitleColor  (0.55f, 0.90f, 1.00f, 1.f);
	const FLinearColor LabelColor  (0.62f, 0.68f, 0.74f, 1.f);
	const FLinearColor ValueColor  (0.94f, 0.94f, 0.94f, 1.f);
	const FLinearColor WarnColor   (1.00f, 0.72f, 0.30f, 1.f);
	const FLinearColor NarrColor   (0.75f, 0.95f, 1.00f, 1.f);
	const FLinearColor TraceColor  (0.45f, 0.50f, 0.56f, 1.f);
	const FLinearColor DegradeColor(1.00f, 0.55f, 0.45f, 1.f);

	constexpr float CardW    = 560.f;
	constexpr float PadX     = 28.f;
	constexpr float LineH    = 26.f;
	constexpr float FadeTime = 0.6f;
}

void ASHMDirectorHUD::ShowDirectorReport(const FPlayerProfile& Profile, const FDirectorDecision& Decision,
	int32 FloorIndex, float Duration)
{
	// 对照组不弹卡——「关掉之后没有任何人在针对我」正是要让观众看到的
	if (!USHMDirectorCore::IsDirectorEnabled())
	{
		return;
	}

	CachedProfile    = Profile;
	CachedDecision   = Decision;
	CachedFloorIndex = FloorIndex;
	ReportDuration   = Duration;
	ReportShownAt    = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bShowingReport   = true;
}

void ASHMDirectorHUD::HideDirectorReport()
{
	bShowingReport = false;
}

void ASHMDirectorHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) { return; }

	DrawDirectorStatusBadge();

	if (!bShowingReport) { return; }

	const float Elapsed = GetWorld()->GetTimeSeconds() - ReportShownAt;
	if (Elapsed > ReportDuration)
	{
		bShowingReport = false;
		return;
	}

	// 末尾 FadeTime 秒淡出，避免硬切
	const float Alpha = (Elapsed > ReportDuration - FadeTime)
		? FMath::Clamp((ReportDuration - Elapsed) / FadeTime, 0.f, 1.f)
		: 1.f;

	DrawReportCard(Alpha);
}

// ============================================================================
//  常驻角标：AI Director ON / OFF
//  录屏时观众能一眼看出这一局是实验组还是对照组——对照演示的可信度全靠它
// ============================================================================
void ASHMDirectorHUD::DrawDirectorStatusBadge()
{
	const bool bOn = USHMDirectorCore::IsDirectorEnabled();
	const FString Text = bOn ? TEXT("AI Director: ON") : TEXT("AI Director: OFF  (对照组)");
	const FLinearColor Color = bOn ? FLinearColor(0.45f, 0.85f, 0.55f, 1.f)
	                               : FLinearColor(0.85f, 0.45f, 0.45f, 1.f);

	DrawLine(Text, 18.f, 16.f, Color, 1.1f);
}

// ============================================================================
//  报告卡本体
// ============================================================================
void ASHMDirectorHUD::DrawReportCard(float Alpha)
{
	const float ScreenW = Canvas->SizeX;
	const float X = (ScreenW - CardW) * 0.5f;
	float Y = 90.f;

	// --- 先量高度：内容行数不固定（规则条数可变），要先算再画底板 ---
	const int32 RuleCount = CachedDecision.RuleModifiers.Num();
	const float CardH = 250.f + FMath::Max(RuleCount, 1) * LineH;

	auto Fade = [Alpha](FLinearColor C) { C.A *= Alpha; return C; };

	// 底板 + 左侧强调条
	DrawRect(Fade(CardBg), X, Y, CardW, CardH);
	DrawRect(Fade(CardBorder), X, Y, 4.f, CardH);

	float TextY = Y + 18.f;
	const float TextX = X + PadX;

	// --- 标题 ---
	TextY += DrawLine(FString::Printf(TEXT("白泽的观察  ·  第 %d 层"), CachedFloorIndex + 1),
		TextX, TextY, Fade(TitleColor), 1.25f);
	TextY += 10.f;

	// --- 观察到什么（画像）---
	TextY += DrawLine(TEXT("我看到的"), TextX, TextY, Fade(LabelColor));
	TextY += DrawLine(FString::Printf(TEXT("  打法集中度 %.0f    效率 %.0f    生存压力 %.0f"),
		CachedProfile.BuildConcentration, CachedProfile.CombatEfficiency, CachedProfile.SurvivalPressure),
		TextX, TextY, Fade(ValueColor));
	TextY += DrawLine(FString::Printf(TEXT("  策略切换 %.0f    判断置信度 %.2f"),
		CachedProfile.StrategySwitch, CachedProfile.Confidence),
		TextX, TextY, Fade(ValueColor));
	TextY += 12.f;

	// --- 改了什么（配比 + 规则）---
	TextY += DrawLine(TEXT("本层调整"), TextX, TextY, Fade(LabelColor));

	FString WeightLine = TEXT("  ");
	for (const TPair<FGameplayTag, float>& Pair : CachedDecision.EnemyWeights)
	{
		FString Short = Pair.Key.GetTagName().ToString();
		Short.RemoveFromStart(TEXT("Enemy."));
		WeightLine += FString::Printf(TEXT("%s %.0f%%   "), *Short, Pair.Value * 100.f);
	}
	TextY += DrawLine(WeightLine, TextX, TextY, Fade(ValueColor));

	if (RuleCount == 0)
	{
		TextY += DrawLine(TEXT("  （本层无规则调整）"), TextX, TextY, Fade(TraceColor));
	}
	for (const FRuleModifier& Mod : CachedDecision.RuleModifiers)
	{
		FString RuleName = Mod.RuleTag.GetTagName().ToString();
		RuleName.RemoveFromStart(TEXT("Rule."));
		TextY += DrawLine(FString::Printf(TEXT("  ⚠ %s  ×%.2f"), *RuleName, Mod.Multiplier),
			TextX, TextY, Fade(WarnColor));
	}
	TextY += 14.f;

	// --- 白泽说什么 ---
	if (!CachedDecision.NarrationLine.IsEmpty())
	{
		TextY += DrawLine(FString::Printf(TEXT("「%s」"), *CachedDecision.NarrationLine),
			TextX, TextY, Fade(NarrColor), 1.1f);
		TextY += 10.f;
	}

	// --- 溯源：这条决策是谁出的、花了多久、降没降级 ---
	// **这一行是三级降级最直观的展示**，别省。观众能直接看到「LLM 挂了但游戏没事」
	const FDirectorTrace& Trace = CachedDecision.Trace;
	FString TraceText = FString::Printf(TEXT("── 出自 %s"), *Trace.ProviderId);
	if (Trace.ElapsedMs > 0.5f)
	{
		TraceText += FString::Printf(TEXT(" · %.0fms"), Trace.ElapsedMs);
	}
	if (Trace.ViolationCount > 0)
	{
		TraceText += FString::Printf(TEXT(" · 护栏拦截 %d 项"), Trace.ViolationCount);
	}
	DrawLine(TraceText, TextX, TextY, Fade(Trace.bDegraded ? DegradeColor : TraceColor), 0.9f);

	if (Trace.bDegraded)
	{
		TextY += LineH * 0.9f;
		DrawLine(FString::Printf(TEXT("   已降级：%s"), *Trace.DegradeReason),
			TextX, TextY, Fade(DegradeColor), 0.9f);
	}
}

float ASHMDirectorHUD::DrawLine(const FString& Text, float X, float Y,
	const FLinearColor& Color, float Scale) const
{
	if (!Canvas) { return LineH; }

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Color.A * 0.6f));
	Canvas->DrawItem(Item);

	return LineH * Scale;
}
