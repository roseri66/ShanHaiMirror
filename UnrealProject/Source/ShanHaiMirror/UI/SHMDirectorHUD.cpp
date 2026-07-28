#include "SHMDirectorHUD.h"
#include "Director/SHMDirectorCore.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

namespace
{
	// 卡片配色：暗底 + 青色主调（对齐「镜界」冷色调，保证白字可读）
	const FLinearColor CardBg      (0.03f, 0.05f, 0.08f, 0.92f);
	const FLinearColor CardBorder  (0.35f, 0.75f, 0.85f, 1.f);
	const FLinearColor TitleColor  (0.55f, 0.90f, 1.00f, 1.f);
	const FLinearColor LabelColor  (0.62f, 0.68f, 0.74f, 1.f);
	const FLinearColor ValueColor  (0.94f, 0.94f, 0.94f, 1.f);
	const FLinearColor WarnColor   (1.00f, 0.72f, 0.30f, 1.f);
	const FLinearColor NarrColor   (0.75f, 0.95f, 1.00f, 1.f);
	const FLinearColor TraceColor  (0.45f, 0.50f, 0.56f, 1.f);
	const FLinearColor DegradeColor(1.00f, 0.55f, 0.45f, 1.f);
	const FLinearColor HintColor   (0.55f, 0.62f, 0.70f, 1.f);

	constexpr float CardW    = 600.f;
	constexpr float PadX     = 28.f;
	constexpr float PadY     = 20.f;
	constexpr float LineH    = 26.f;
	constexpr float FadeTime = 0.4f;

	// 「继续」键：刻意不用空格——空格已绑定武器切换，会误触发
	bool IsContinuePressed(const APlayerController* PC)
	{
		if (!PC) { return false; }
		return PC->IsInputKeyDown(EKeys::Enter)
			|| PC->IsInputKeyDown(EKeys::E)
			|| PC->IsInputKeyDown(EKeys::LeftMouseButton);
	}
}

void ASHMDirectorHUD::ShowDirectorReport(const FPlayerProfile& Profile, const FDirectorDecision& Decision,
	int32 FloorIndex)
{
	// 对照组不弹卡——「关掉之后没有任何人在针对我」正是要让观众看到的
	if (!USHMDirectorCore::IsDirectorEnabled())
	{
		return;
	}

	CachedProfile    = Profile;
	CachedDecision   = Decision;
	CachedFloorIndex = FloorIndex;
	ReportShownAt    = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bShowingReport   = true;
}

void ASHMDirectorHUD::HideDirectorReport()
{
	bShowingReport = false;
}

float ASHMDirectorHUD::GetReportElapsed() const
{
	if (!bShowingReport || !GetWorld()) { return 0.f; }
	return GetWorld()->GetTimeSeconds() - ReportShownAt;
}

bool ASHMDirectorHUD::IsReportAcknowledged() const
{
	if (!bShowingReport) { return true; }

	const float Elapsed = GetReportElapsed();
	if (Elapsed < MinReadSeconds)
	{
		return false;   // 防手滑：最短阅读时间内不接受跳过
	}
	return Elapsed >= AutoDismissSeconds || IsContinuePressed(PlayerOwner);
}

void ASHMDirectorHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) { return; }

	DrawDirectorStatusBadge();

	if (!bShowingReport) { return; }

	const float Elapsed = GetReportElapsed();
	const float Remain  = AutoDismissSeconds - Elapsed;
	const float Alpha   = (Remain < FadeTime) ? FMath::Clamp(Remain / FadeTime, 0.f, 1.f) : 1.f;

	DrawReportCard(Alpha);
}

// ============================================================================
//  常驻角标：AI Director 状态 + 当前 Provider
//
//  必须把 Provider 也显示出来：**AI 导演 ≠ LLM**。本地 Provider 一样在读画像、
//  做定向反制——这正是「断网完整可玩」的含义。只写 ON/OFF 会让人误以为
//  「没接 LLM 就等于导演没开」（实测收到过这个反馈）。
// ============================================================================
void ASHMDirectorHUD::DrawDirectorStatusBadge()
{
	const bool bOn = USHMDirectorCore::IsDirectorEnabled();

	FString Text;
	FLinearColor Color;

	if (bOn)
	{
		FString ProviderName = TEXT("Local");
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (const USHMDirectorCore* Core = GI->GetSubsystem<USHMDirectorCore>())
				{
					ProviderName = Core->GetProviderName();
				}
			}
		}
		const bool bLlm = ProviderName == TEXT("Llm");
		Text  = FString::Printf(TEXT("AI Director: ON  ·  决策来源 %s%s"),
			*ProviderName, bLlm ? TEXT("") : TEXT("（LLM 未接入，本地规则表决策）"));
		Color = bLlm ? FLinearColor(0.45f, 0.85f, 0.95f, 1.f)
		             : FLinearColor(0.45f, 0.85f, 0.55f, 1.f);
	}
	else
	{
		Text  = TEXT("AI Director: OFF  ·  对照组（固定配比，无针对）");
		Color = FLinearColor(0.85f, 0.45f, 0.45f, 1.f);
	}

	DrawLine(Text, 18.f, 16.f, Color, 1.05f);
}

// ============================================================================
//  内容排版 —— 先排行表，再据此算高度
// ============================================================================
void ASHMDirectorHUD::BuildReportLines(TArray<FReportLine>& Out) const
{
	Out.Add({ FString::Printf(TEXT("白泽的观察  ·  第 %d 层"), CachedFloorIndex + 1),
		TitleColor, 1.25f, 12.f });

	// --- 我看到了什么 ---
	Out.Add({ TEXT("我看到的"), LabelColor, 1.f, 0.f });
	Out.Add({ FString::Printf(TEXT("   打法集中度 %.0f     战斗效率 %.0f     生存压力 %.0f"),
		CachedProfile.BuildConcentration, CachedProfile.CombatEfficiency,
		CachedProfile.SurvivalPressure), ValueColor, 1.f, 0.f });
	Out.Add({ FString::Printf(TEXT("   策略切换 %.0f     判断置信度 %.2f"),
		CachedProfile.StrategySwitch, CachedProfile.Confidence), ValueColor, 1.f, 14.f });

	// --- 我改了什么 ---
	Out.Add({ TEXT("本层调整"), LabelColor, 1.f, 0.f });

	FString WeightLine = TEXT("   ");
	for (const TPair<FGameplayTag, float>& Pair : CachedDecision.EnemyWeights)
	{
		FString Short = Pair.Key.GetTagName().ToString();
		Short.RemoveFromStart(TEXT("Enemy."));
		WeightLine += FString::Printf(TEXT("%s %.0f%%    "), *Short, Pair.Value * 100.f);
	}
	Out.Add({ WeightLine, ValueColor, 1.f, 0.f });

	if (CachedDecision.RuleModifiers.Num() == 0)
	{
		Out.Add({ TEXT("   （本层无规则调整）"), TraceColor, 1.f, 14.f });
	}
	else
	{
		for (int32 i = 0; i < CachedDecision.RuleModifiers.Num(); ++i)
		{
			const FRuleModifier& Mod = CachedDecision.RuleModifiers[i];
			FString RuleName = Mod.RuleTag.GetTagName().ToString();
			RuleName.RemoveFromStart(TEXT("Rule."));
			const bool bLast = (i == CachedDecision.RuleModifiers.Num() - 1);
			Out.Add({ FString::Printf(TEXT("   [!] %s  x%.2f"), *RuleName, Mod.Multiplier),
				WarnColor, 1.f, bLast ? 14.f : 0.f });
		}
	}

	// --- 白泽说什么 ---
	if (!CachedDecision.NarrationLine.IsEmpty())
	{
		Out.Add({ FString::Printf(TEXT("「%s」"), *CachedDecision.NarrationLine),
			NarrColor, 1.1f, 12.f });
	}

	// --- 溯源：三级降级最直观的展示，不省 ---
	const FDirectorTrace& Trace = CachedDecision.Trace;
	FString TraceText = FString::Printf(TEXT("-- 出自 %s"), *Trace.ProviderId);
	if (Trace.ElapsedMs > 0.5f)   { TraceText += FString::Printf(TEXT("  ·  %.0fms"), Trace.ElapsedMs); }
	if (Trace.ViolationCount > 0) { TraceText += FString::Printf(TEXT("  ·  护栏拦截 %d 项"), Trace.ViolationCount); }
	Out.Add({ TraceText, Trace.bDegraded ? DegradeColor : TraceColor, 0.9f, 0.f });

	if (Trace.bDegraded)
	{
		Out.Add({ FString::Printf(TEXT("   已降级：%s"), *Trace.DegradeReason),
			DegradeColor, 0.9f, 0.f });
	}

	// --- 继续提示（含倒计时，让玩家知道不按也会走）---
	const float Remain = FMath::Max(0.f, AutoDismissSeconds - GetReportElapsed());
	const FString Hint = (GetReportElapsed() < MinReadSeconds)
		? TEXT("")
		: FString::Printf(TEXT("按 E / 回车 / 左键 继续      （%.0fs 后自动开始）"), Remain);
	if (!Hint.IsEmpty())
	{
		Out.Add({ Hint, HintColor, 0.9f, 0.f });
	}
}

void ASHMDirectorHUD::DrawReportCard(float Alpha)
{
	TArray<FReportLine> Lines;
	BuildReportLines(Lines);

	// 由内容算高度，绝不写死
	float ContentH = 0.f;
	for (const FReportLine& L : Lines)
	{
		ContentH += LineH * L.Scale + L.PadBelow;
	}
	const float CardH = ContentH + PadY * 2.f;

	const float X = (Canvas->SizeX - CardW) * 0.5f;
	const float Y = FMath::Max(70.f, (Canvas->SizeY - CardH) * 0.28f);

	auto Fade = [Alpha](FLinearColor C) { C.A *= Alpha; return C; };

	DrawRect(Fade(CardBg),     X, Y, CardW, CardH);
	DrawRect(Fade(CardBorder), X, Y, 4.f,   CardH);   // 左侧强调条

	float TextY = Y + PadY;
	for (const FReportLine& L : Lines)
	{
		DrawLine(L.Text, X + PadX, TextY, Fade(L.Color), L.Scale);
		TextY += LineH * L.Scale + L.PadBelow;
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
