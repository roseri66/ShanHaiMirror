#include "SHMBehaviorRecorder.h"
#include "Framework/SHMEventBus.h"
#include "Framework/SHMGameplayTags.h"

void USHMBehaviorRecorder::Initialize(FSubsystemCollectionBase& Collection)
{
	// 显式声明依赖：保证 EventBus 先于本子系统初始化，
	// 否则下面 GetSubsystem 可能拿到 nullptr（子系统初始化顺序不保证）
	Collection.InitializeDependency<USHMEventBus>();

	Super::Initialize(Collection);

	if (USHMEventBus* Bus = GetGameInstance()->GetSubsystem<USHMEventBus>())
	{
		Bus->OnGameplayEvent.AddDynamic(this, &USHMBehaviorRecorder::HandleGameplayEvent);
	}

	ResetRun();
}

void USHMBehaviorRecorder::Deinitialize()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USHMEventBus* Bus = GI->GetSubsystem<USHMEventBus>())
		{
			Bus->OnGameplayEvent.RemoveDynamic(this, &USHMBehaviorRecorder::HandleGameplayEvent);
		}
	}

	Super::Deinitialize();
}

void USHMBehaviorRecorder::HandleGameplayEvent(const FSHMGameplayEvent& Event)
{
	RecordEvent(Event);
}

// ============================================================================
//  核心：事件 → 快照累积
//  这里只有加法和计数，没有一个 if 是在判断"玩家打得好不好"
// ============================================================================
void USHMBehaviorRecorder::RecordEvent(const FSHMGameplayEvent& Event)
{
	const FGameplayTag& Tag = Event.EventTag;

	// --- 维度1：Build 集中度 ---
	if (Tag == SHMTags::Event_Combat_Attack)
	{
		if (Event.SourceTag.IsValid())
		{
			Current.WeaponAttackCounts.FindOrAdd(Event.SourceTag)++;
			Current.UniqueWeaponsUsed.Add(Event.SourceTag);
		}
	}
	else if (Tag == SHMTags::Event_Combat_SkillCast)
	{
		if (Event.SourceTag.IsValid())
		{
			Current.SkillCastCounts.FindOrAdd(Event.SourceTag)++;
			Current.UniqueSkillsUsed.Add(Event.SourceTag);
		}
	}
	else if (Tag == SHMTags::Event_Combat_DamageDealt)
	{
		if (Event.SourceTag.IsValid())
		{
			Current.DamageByTag.FindOrAdd(Event.SourceTag) += Event.Magnitude;
		}
	}
	else if (Tag == SHMTags::Event_Combat_Kill)
	{
		if (Event.SourceTag.IsValid())
		{
			Current.KillsByTag.FindOrAdd(Event.SourceTag)++;
		}
	}
	// --- 维度2：战斗效率 ---
	else if (Tag == SHMTags::Event_Combat_HitTaken)
	{
		Current.HitsTaken++;
		Current.TotalDamageTaken += Event.Magnitude;
	}
	else if (Tag == SHMTags::Event_Combat_Dodge)
	{
		Current.DodgeAttempts++;
		if (Event.bSuccess)
		{
			Current.DodgeSuccesses++;
		}
	}
	else if (Tag == SHMTags::Event_Flow_RoomFinished)
	{
		Current.RoomsCleared++;
		// 耗时由广播方传入（Magnitude），本类不读世界时间——这是可测性的前提
		Current.TotalRoomClearTime += Event.Magnitude;
	}
	// --- 维度4：策略切换意愿 ---
	else if (Tag == SHMTags::Event_Combat_WeaponSwitch)
	{
		Current.WeaponSwitchCount++;
		// 切换后的新武器也算"用过"——哪怕还没来得及攻击
		if (Event.SourceTag.IsValid())
		{
			Current.UniqueWeaponsUsed.Add(Event.SourceTag);
		}
	}
	// --- 维度5：生存压力 ---
	else if (Tag == SHMTags::Event_Combat_LowHp)
	{
		Current.LowHpEvents++;
	}
	else if (Tag == SHMTags::Event_Combat_Death)
	{
		Current.DeathOrNearDeath++;
	}
	// 其余事件（Event.Item.Use / Event.Flow.FloorFinished 等）当前不参与画像，
	// 忽略即可——不要在这里"顺手"做别的事，边界一破就再也收不回来
}

// ============================================================================
//  层生命周期
// ============================================================================
void USHMBehaviorRecorder::BeginFloor(int32 FloorIndex)
{
	Current = FFloorBehaviorSnapshot();
	Current.FloorIndex = FloorIndex;
}

const FFloorBehaviorSnapshot& USHMBehaviorRecorder::FinalizeFloor()
{
	History.Add(Current);
	return History.Last();
}

void USHMBehaviorRecorder::ResetRun()
{
	History.Empty();
	BeginFloor(0);
}

// ============================================================================
//  调试
// ============================================================================
FString USHMBehaviorRecorder::DebugSnapshotToString() const
{
	FString Out = FString::Printf(TEXT("[F%d] 攻击:"), Current.FloorIndex);

	for (const TPair<FGameplayTag, int32>& Pair : Current.WeaponAttackCounts)
	{
		Out += FString::Printf(TEXT(" %s=%d"), *Pair.Key.GetTagName().ToString(), Pair.Value);
	}

	Out += FString::Printf(
		TEXT(" | 受击:%d(%.0f伤害) 闪避:%d/%d 切武器:%d 残血:%d 房间:%d(%.1fs)"),
		Current.HitsTaken, Current.TotalDamageTaken,
		Current.DodgeSuccesses, Current.DodgeAttempts,
		Current.WeaponSwitchCount, Current.LowHpEvents,
		Current.RoomsCleared, Current.TotalRoomClearTime);

	return Out;
}
