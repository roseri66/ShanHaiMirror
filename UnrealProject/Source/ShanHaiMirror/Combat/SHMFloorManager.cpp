#include "SHMFloorManager.h"
#include "SHMEncounterManager.h"
#include "Director/SHMBehaviorRecorder.h"
#include "Director/SHMProfileAnalyzer.h"
#include "Director/SHMDirectorCore.h"
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

	// F0 的决策 = 观察层（DirectorCore 内部短路），画像给空的即可
	CurrentDecision = Director->DecideForFloor(FPlayerProfile(), 0);
	ShowDirectorMessage(CurrentDecision);

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
		return;
	}

	CurrentDecision = Director->DecideForFloor(Profile, FloorIndex);
	ShowDirectorMessage(CurrentDecision);

	GetWorld()->GetTimerManager().SetTimer(DelayTimer, this, &USHMFloorManager::StartFloor, 2.5f, false);
}

void USHMFloorManager::ShowDirectorMessage(const FDirectorDecision& Decision) const
{
	// 正式导演报告 UI 是第五次开工的事；屏显足以让"被针对"可感、可录屏
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1001, 8.f, FColor::Cyan,
			FString::Printf(TEXT("【白泽】%s"), *Decision.NarrationLine));
		GEngine->AddOnScreenDebugMessage(1003, 8.f, FColor::Silver,
			FString::Printf(TEXT("（%s）"), *Decision.Reason));
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
