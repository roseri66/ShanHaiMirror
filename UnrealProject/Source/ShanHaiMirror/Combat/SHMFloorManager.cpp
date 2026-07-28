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

	// 每层第一个房间之前有两道等待，都必须过了才刷怪：
	//   ① 决策还在路上（LLM 往返）——不等就会用垫底配比刷怪，那次调用白费
	//   ② 报告卡还没读完——不等就是「怪已经打上来了玩家还在看卡」（实测反馈）
	// 二者都有封顶，绝不会把流程卡死。
	if (RoomIndex == 0)
	{
		// ① 等决策
		if (bDecisionPending)
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

		// ② 等玩家读完报告卡
		if (ASHMDirectorHUD* Hud = GetDirectorHUD())
		{
			if (Hud->IsReportShowing() && !Hud->IsReportAcknowledged())
			{
				GetWorld()->GetTimerManager().SetTimer(DelayTimer, this,
					&USHMFloorManager::StartRoom, DecisionPollSeconds, false);
				return;
			}
			Hud->HideDirectorReport();   // 读完即收，战斗界面保持干净
		}
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

	// 报告卡：层间弹出。StartRoom 会等它被读完才刷怪——
	// 这既让玩家有时间读，也顺带把 LLM 的 4~6 秒延迟吸收干净
	if (ASHMDirectorHUD* Hud = GetDirectorHUD())
	{
		Hud->ShowDirectorReport(LastProfile, Decision, FloorIndex);
	}

	UE_LOG(LogSHMFloor, Log, TEXT("应用决策：\n%s"), *USHMDirectorCore::DecisionToString(Decision));
}

ASHMDirectorHUD* USHMFloorManager::GetDirectorHUD() const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	return PC ? Cast<ASHMDirectorHUD>(PC->GetHUD()) : nullptr;
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
