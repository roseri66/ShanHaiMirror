#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Director/SHMDirectorCore.h"
#include "Director/SHMAIProvider.h"
#include "Framework/SHMGameplayTags.h"

// ============================================================================
// 三级降级链路测试
//
// 为什么这一组值得单独存在：**"第 ④ 步可以整体失败" 是本项目四个不变量之一**，
// 也是"断网/无 key 完整可玩"这个卖点的全部依据。在此之前它只靠人工验收 ——
// 而人工验收覆盖不到"Provider 返回畸形数据""本地兜底也被拒"这类路径。
//
// 三级降级（SHMDirectorCore::FinishDecision）：
//   ① Provider 交不出结果        → 本地重新决策
//   ② Provider 交出了但被护栏拒   → 本地重新决策，且**日志留下被拒的原件**
//   ③ 连本地产出都过不了护栏      → 安全兜底（不该发生，但必须有）
//
// 每条路径的判据都不只是"没崩"，而是**降级后的语义正确**：
// 是否标记了 degraded、原因是否可读、被拒的原件有没有留下。
// ============================================================================

namespace
{
	// UE 5.5 的 EAutomationTestFlags 是 enum class，不能退化成 int32
	constexpr EAutomationTestFlags DegradeTestFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	// --- 测试替身：永远交不出结果（模拟超时/断网/HTTP 错）---
	class FAlwaysFailingProvider : public ISHMAIProvider
	{
	public:
		virtual void RequestIntentAsync(const FDirectorContext&, FSHMOnIntentReady OnDone) override
		{
			OnDone.ExecuteIfBound(FDirectorIntent(), false);
		}
		virtual FString GetProviderName() const override { return TEXT("AlwaysFailing"); }
	};

	// --- 测试替身：交出一个必被 Conflict 护栏拒绝的 Intent ---
	// 弹药↓ + 远程伤害↓ 对远程玩家是无解组合，正是 2026-07-28 DeepSeek 实测撞出的那条。
	class FConflictingProvider : public ISHMAIProvider
	{
	public:
		virtual void RequestIntentAsync(const FDirectorContext&, FSHMOnIntentReady OnDone) override
		{
			FDirectorIntent Intent;
			Intent.ChallengeLevel = EChallengeLevel::Counter;
			Intent.EnemyWeights.Add(SHMTags::Enemy_Grunt.GetTag(),   0.45f);
			Intent.EnemyWeights.Add(SHMTags::Enemy_Tank.GetTag(),    0.25f);
			Intent.EnemyWeights.Add(SHMTags::Enemy_Rush.GetTag(),    0.20f);
			Intent.EnemyWeights.Add(SHMTags::Enemy_Shooter.GetTag(), 0.10f);

			FRuleIntent Ammo;
			Ammo.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.Ammo"));
			Ammo.Level   = TEXT("light");
			FRuleIntent Ranged;
			Ranged.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Rule.RangedDamage"));
			Ranged.Level   = TEXT("light");
			Intent.RuleIntents.Add(Ammo);
			Intent.RuleIntents.Add(Ranged);

			Intent.Narration = TEXT("你的箭袋会更浅，你的箭也会更钝。");
			Intent.Reason    = TEXT("测试替身：必被 Conflict 护栏拒绝的组合。");
			OnDone.ExecuteIfBound(Intent, true);
		}
		virtual FString GetProviderName() const override { return TEXT("Conflicting"); }
	};

	// 一个高置信度的远程画像：能让本地 Provider 走到 Counter 分支
	FPlayerProfile MakeDegradeRangerProfile()
	{
		FPlayerProfile P;
		P.BuildConcentration = 90.f;
		P.CombatEfficiency   = 85.f;
		P.ResourceSurplus    = 50.f;
		P.StrategySwitch     = 5.f;
		P.SurvivalPressure   = 10.f;
		P.Confidence         = 0.9f;
		P.DominantArchetype  = SHMTags::Archetype_Ranger.GetTag();
		P.PrimaryBuildTags   = { SHMTags::Build_Ranged.GetTag() };
		return P;
	}

	// 造一个装好指定 Provider 的 DirectorCore。
	// GameInstanceSubsystem 的 ClassWithin 是 UGameInstance，不能挂 transient package。
	USHMDirectorCore* MakeDegradeCore(TUniquePtr<ISHMAIProvider> Provider)
	{
		UGameInstance* TempGI = NewObject<UGameInstance>(GEngine);
		USHMDirectorCore* Core = NewObject<USHMDirectorCore>(TempGI);
		Core->SetupForTesting(MoveTemp(Provider));
		return Core;
	}

	// 同步取一次决策：三个测试替身都在调用内立即回调，所以这样是安全的
	bool DegradeDecideSync(USHMDirectorCore* Core, int32 FloorIndex, FDirectorDecision& Out)
	{
		bool bGot = false;
		Core->DecideForFloorAsync(MakeDegradeRangerProfile(), FloorIndex,
			USHMDirectorCore::FSHMOnDecisionReady::CreateLambda(
				[&Out, &bGot](const FDirectorDecision& D) { Out = D; bGot = true; }));
		return bGot;
	}
}

// ---------------------------------------------------------------------------
// 降级①：Provider 交不出结果 → 本地重新决策
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMDegradeProviderFailsTest,
	"SHM.Director.Degrade.ProviderFails_FallsBackToLocal", DegradeTestFlags)
bool FSHMDegradeProviderFailsTest::RunTest(const FString& Parameters)
{
	USHMDirectorCore* Core = MakeDegradeCore(MakeUnique<FAlwaysFailingProvider>());

	FDirectorDecision Decision;
	if (!TestTrue(TEXT("回调必定被调用一次"), DegradeDecideSync(Core, 1, Decision)))
	{
		return false;
	}

	TestTrue(TEXT("应标记为已降级"), Decision.Trace.bDegraded);
	TestTrue(TEXT("降级原因不应为空——静默降级会让人以为 LLM 生效了"),
		!Decision.Trace.DegradeReason.IsEmpty());
	TestEqual(TEXT("溯源应保留原 Provider 名，而不是改写成 Local"),
		Decision.Trace.ProviderId, TEXT("AlwaysFailing"));

	// 关键：降级之后游戏仍然拿得到一份可用决策，这正是"断网完整可玩"的含义
	TestTrue(TEXT("降级后仍须产出敌人配比，否则这一层无怪可刷"),
		Decision.EnemyWeights.Num() > 0);

	// 这条路径没有"被拦的原件"——DirectorCore 直接拿本地 Intent 走后续流程，
	// 所以护栏是干净的。前端据此区分两种降级（degradeKind）。
	TestEqual(TEXT("Provider 无输出属于降级①，不应产生护栏违规"),
		Decision.Trace.ViolationCount, 0);
	return true;
}

// ---------------------------------------------------------------------------
// 降级②：Provider 交出了，但被护栏拒 → 本地重新决策 + 留下被拒的原件
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMDegradeGuardrailRejectsTest,
	"SHM.Director.Degrade.GuardrailRejects_FallsBackAndKeepsRawIntent", DegradeTestFlags)
bool FSHMDegradeGuardrailRejectsTest::RunTest(const FString& Parameters)
{
	USHMDirectorCore* Core = MakeDegradeCore(MakeUnique<FConflictingProvider>());

	FDirectorDecision Decision;
	if (!TestTrue(TEXT("回调必定被调用一次"), DegradeDecideSync(Core, 1, Decision)))
	{
		return false;
	}

	TestTrue(TEXT("护栏拒绝后应标记为已降级"), Decision.Trace.bDegraded);
	TestTrue(TEXT("应记录护栏拦截数——这是分道统计与前端灯带的数据源"),
		Decision.Trace.ViolationCount > 0);

	// **这条是整个项目最值钱的断言**：被拒的原件必须留在日志里，
	// 否则"LLM 想改什么 → 护栏拦没拦 → 实际改了什么"这一屏就无从渲染。
	const TArray<FSHMFloorRecord>& Records = Core->GetFloorRecords();
	if (!TestEqual(TEXT("应留下一条层记录"), Records.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("层记录里的护栏前规则必须是**被拒的原件**（2 条），不是降级后的结果"),
		Records[0].RawIntentRules.Num(), 2);
	TestTrue(TEXT("应记下是哪几道护栏拦的——时间轴与前端灯带靠它"),
		Records[0].TriggeredGuards.Num() > 0);

	// 生效的决策来自本地重新决策，与被拒的那份不同
	TestTrue(TEXT("降级后仍须产出敌人配比"), Decision.EnemyWeights.Num() > 0);
	return true;
}

// ---------------------------------------------------------------------------
// 观察层短路：F0 不经 Provider（即便 Provider 会失败也不受影响）
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMObserveFloorTest,
	"SHM.Director.Degrade.ObserveFloor_BypassesProvider", DegradeTestFlags)
bool FSHMObserveFloorTest::RunTest(const FString& Parameters)
{
	// 故意塞一个必失败的 Provider：若首层真去问了它，就会被标记成降级
	USHMDirectorCore* Core = MakeDegradeCore(MakeUnique<FAlwaysFailingProvider>());

	FDirectorDecision Decision;
	if (!TestTrue(TEXT("回调必定被调用一次"), DegradeDecideSync(Core, 0, Decision)))
	{
		return false;
	}

	TestEqual(TEXT("首层应由代码短路，溯源标 ObserveFloor"),
		Decision.Trace.ProviderId, TEXT("ObserveFloor"));
	TestFalse(TEXT("首层不调用 Provider，因此不该被标记为降级"), Decision.Trace.bDegraded);
	TestEqual(TEXT("首层不做任何规则调整（GDD：只观察）"),
		Decision.RuleModifiers.Num(), 0);
	TestTrue(TEXT("首层仍须给出配比，否则无怪可刷"), Decision.EnemyWeights.Num() > 0);
	return true;
}

// ---------------------------------------------------------------------------
// 回调契约：任何路径下回调都恰好被调用一次
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHMCallbackExactlyOnceTest,
	"SHM.Director.Degrade.Callback_FiresExactlyOnceOnEveryPath", DegradeTestFlags)
bool FSHMCallbackExactlyOnceTest::RunTest(const FString& Parameters)
{
	// 接口注释承诺"回调必定被调用一次，调用方不需要处理没回调的情况"。
	// 多调一次同样是 bug：FloorManager 会把一层的决策应用两遍。
	int32 FailCount = 0;
	for (int32 Floor : { 0, 1, 2 })
	{
		{
			USHMDirectorCore* Core = MakeDegradeCore(MakeUnique<FAlwaysFailingProvider>());
			int32 Calls = 0;
			Core->DecideForFloorAsync(MakeDegradeRangerProfile(), Floor,
				USHMDirectorCore::FSHMOnDecisionReady::CreateLambda(
					[&Calls](const FDirectorDecision&) { ++Calls; }));
			if (!TestEqual(*FString::Printf(TEXT("失败 Provider · F%d 应恰好回调一次"), Floor), Calls, 1))
			{
				++FailCount;
			}
		}
		{
			USHMDirectorCore* Core = MakeDegradeCore(MakeUnique<FConflictingProvider>());
			int32 Calls = 0;
			Core->DecideForFloorAsync(MakeDegradeRangerProfile(), Floor,
				USHMDirectorCore::FSHMOnDecisionReady::CreateLambda(
					[&Calls](const FDirectorDecision&) { ++Calls; }));
			if (!TestEqual(*FString::Printf(TEXT("被拒 Provider · F%d 应恰好回调一次"), Floor), Calls, 1))
			{
				++FailCount;
			}
		}
	}
	return FailCount == 0;
}
