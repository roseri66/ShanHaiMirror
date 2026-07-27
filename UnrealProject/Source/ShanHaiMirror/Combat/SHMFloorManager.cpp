#include "SHMFloorManager.h"
#include "SHMEncounterManager.h"
#include "Director/SHMBehaviorRecorder.h"
#include "Director/SHMProfileAnalyzer.h"
#include "Director/SHMDirectorCore.h"
#include "UI/SHMDirectorHUD.h"
#include "Framework/SHMEventBus.h"
#include "Framework/SHMGameplayTags.h"
#include "Framework/SHMAttributeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHMFloor, Log, All);

void USHMFloorManager::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 延迟启动：等 GameMode 生成并 Possess 玩家（踩坑 #4 的教训——别赌初始化顺序）
	InWorld.GetTimerManager().SetTimer(DelayTimer, this, &USHMFloorManager::StartRun, 2.f, false);
}

void USHMFloorManager::StartRun()
{
	UGameInstance* GI = GetWorld()->GetGameInstance();
	USHMBehaviorRecorder* Recorder = GI ? GI->GetSubsystem<USHMBehaviorRecorder>() : nullptr;
	USHMDirectorCore*     Director = GI ? GI->GetSubsystem<USHMDirectorCore>()     : nullptr;
	if (!Recorder || !Director)
	{
		UE_LOG(LogSHMFloor, Error, TEXT("StartRun: 子系统缺失，流程不启动"));
		return;
	}

	Recorder->ResetRun();
	Director->ResetRun();
	FloorIndex = 0;

	// F0 = 观察层（DirectorCore 内部短路，同步返回），画像给空的即可
	Director->DecideForFloorAsync(FPlayerProfile(), 0,
		USHMDirectorCore::FSHMOnDecisionReady::CreateWeakLambda(this,
			[this](const FDirectorDecision& Decision)
	{
		CurrentDecision = Decision;
		ShowDirectorMessage(Decision);
	}));

	StartFloor();
}

void USHMFloorManager::StartFloor()
{
	if (USHMBehaviorRecorder* Recorder = GetWorld()->GetGameInstance()->GetSubsystem<USHMBehaviorRecorder>())
	{
		Recorder->BeginFloor(FloorIndex);
	}
	RoomIndex = 0;
	StartRoom();
}

void USHMFloorManager::StartRoom()
{
	if (IsPlayerDead()) { return; }

	// 决策还在路上就再等等——过场时长是「最少等多久」，不是「最多等多久」。
	// 实测 DeepSeek 单次往返约 3.8s，超过 2.5s 过场；若按时开打，这一层会用垫底
	// 配比刷怪，LLM 那次调用就白费了。等待封顶 MaxDecisionWaitSeconds 后照常开打。
	if (bDecisionPending && RoomIndex == 0)
	{
		if (DecisionWaitedTime < MaxDecisionWaitSeconds)
		{
			DecisionWaitedTime += DecisionPollSeconds;
			GetWorld()->GetTimerManager().SetTimer(DelayTimer, this,
				&USHMFloorManager::StartRoom, DecisionPollSeconds, false);
			return;
		}

		UE_LOG(LogSHMFloor, Warning,
			TEXT("等待决策超过 %.1fs，用垫底决策开打（这一层不会体现 AI 调整）"),
			MaxDecisionWaitSeconds);
		bDecisionPending = false;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	USHMEncounterManager* Encounter = GetWorld()->GetSubsystem<USHMEncounterManager>();
	if (!Player || !Encounter)
	{
		UE_LOG(LogSHMFloor, Error, TEXT("StartRoom: 玩家或遭遇管理器缺失"));
		return;
	}

	RoomStartTime = GetWorld()->GetTimeSeconds();

	Encounter->OnRoomCleared.Clear();
	Encounter->OnRoomCleared.AddUObject(this, &USHMFloorManager::HandleRoomCleared);

	const int32 Spawned = Encounter->SpawnRoomWave(
		CurrentDecision.EnemyWeights, RoomThreatBudget(FloorIndex), Player->GetActorLocation());

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1002, 4.f, FColor::White,
			FString::Printf(TEXT("—— F%d · 房间 %d/%d ——"), FloorIndex + 1, RoomIndex + 1, RoomsPerFloor));
	}

	if (Spawned == 0)
	{
		// 数据缺失时不卡死流程：直接视为清空推进（错误已由 Encounter 打日志）
		HandleRoomCleared();
	}
}

void USHMFloorManager::HandleRoomCleared()
{
	if (IsPlayerDead()) { return; }

	// 真实耗时进画像——战斗效率维度从此不再是中性 50
	const float Elapsed = GetWorld()->GetTimeSeconds() - RoomStartTime;
	if (USHMEventBus* Bus = USHMEventBus::Get(GetWorld()->GetGameInstance()))
	{
		Bus->BroadcastSimple(SHMTags::Event_Flow_RoomFinished,
			UGameplayStatics::GetPlayerPawn(GetWorld(), 0),
			FGameplayTag(), FGameplayTag(), Elapsed);
	}

	++RoomIndex;
	if (RoomIndex < RoomsPerFloor)
	{
		GetWorld()->GetTimerManager().SetTimer(DelayTimer, this, &USHMFloorManager::StartRoom, 2.f, false);
	}
	else
	{
		EndFloor();
	}
}

void USHMFloorManager::EndFloor()
{
	UGameInstance* GI = GetWorld()->GetGameInstance();
	USHMBehaviorRecorder* Recorder = GI->GetSubsystem<USHMBehaviorRecorder>();
	USHMDirectorCore*     Director = GI->GetSubsystem<USHMDirectorCore>();

	// 契约：Analyzer 的 History 不含当前层——先分析后定稿，顺序不能反
	const FPlayerProfile Profile =
		FSHMProfileAnalyzer::Analyze(Recorder->GetCurrentSnapshot(), Recorder->GetHistory());
	Recorder->FinalizeFloor();
	LastProfile = Profile;   // 报告卡的「我看到了什么」数据源

	if (USHMEventBus* Bus = USHMEventBus::Get(GI))
	{
		Bus->BroadcastSimple(SHMTags::Event_Flow_FloorFinished, nullptr,
			FGameplayTag(), FGameplayTag(), static_cast<float>(FloorIndex));
	}

	++FloorIndex;
	if (FloorIndex >= TotalFloors)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1001, 15.f, FColor::Green,
				TEXT("【白泽】镜之试炼到此为止。你走过的每一步，我都记下了。（一局完成）"));
		}
		UE_LOG(LogSHMFloor, Log, TEXT("一局完成（%d 层）"), TotalFloors);

		// 导出整局决策日志：观察→判断→改了什么，一份可回放、可给前端渲染的证据
		const FString DecisionLogPath = FPaths::ProjectSavedDir() /
			FString::Printf(TEXT("DecisionLogs/Run_%s.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		Director->ExportDecisionLog(DecisionLogPath);
		return;
	}

	// 异步决策：LLM 往返被层间过场掩盖。先用观察层决策垫底，万一等待超时也不会空决策。
	// 回调必定触发一次（DirectorCore 内部三级降级保证），不需要"没回调"的分支。
	CurrentDecision    = USHMDirectorCore::MakeObserveFloorDecision();
	bDecisionPending   = true;
	DecisionWaitedTime = 0.f;

	Director->DecideForFloorAsync(Profile, FloorIndex,
		USHMDirectorCore::FSHMOnDecisionReady::CreateWeakLambda(this,
			[this](const FDirectorDecision& Decision)
	{
		CurrentDecision  = Decision;
		bDecisionPending = false;
		ShowDirectorMessage(Decision);
	}));

	GetWorld()->GetTimerManager().SetTimer(DelayTimer, this,
		&USHMFloorManager::StartFloor, FloorTransitionSeconds, false);
}

void USHMFloorManager::ShowDirectorMessage(const FDirectorDecision& Decision) const
{
	// 对照组（导演关闭）：白泽是沉默的，什么都不显示。
	// 这不是"少显示一条信息"，而是对照实验的一部分——观众要看到的正是
	// 「关掉之后没有任何人在针对我」。
	if (!USHMDirectorCore::IsDirectorEnabled())
	{
		UE_LOG(LogSHMFloor, Log, TEXT("【对照组】导演关闭，本层无调整"));
		return;
	}

	// 报告卡：层间弹出，同时吸收 LLM 的 4~6 秒延迟
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (ASHMDirectorHUD* Hud = Cast<ASHMDirectorHUD>(PC->GetHUD()))
		{
			Hud->ShowDirectorReport(LastProfile, Decision, FloorIndex);
		}
	}

	UE_LOG(LogSHMFloor, Log, TEXT("应用决策：\n%s"), *USHMDirectorCore::DecisionToString(Decision));
}

bool USHMFloorManager::IsPlayerDead() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const USHMAttributeComponent* Attr = Player ? Player->FindComponentByClass<USHMAttributeComponent>() : nullptr;
	if (Attr && Attr->IsDead())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1001, 10.f, FColor::Red, TEXT("【白泽】试炼结束。镜面重归平静。"));
		}
		return true;
	}
	return false;
}
