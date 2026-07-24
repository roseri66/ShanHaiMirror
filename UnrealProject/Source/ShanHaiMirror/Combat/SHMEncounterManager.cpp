#include "SHMEncounterManager.h"
#include "SHMEncounterMath.h"
#include "Enemies/SHMEnemy.h"
#include "Enemies/SHMEnemyTable.h"
#include "Enemies/SHMAIController.h"
#include "Framework/SHMCsvTable.h"
#include "Framework/SHMAttributeComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHMEncounter, Log, All);

void USHMEncounterManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EnemyTable = FSHMCsvTable::LoadProjectData(TEXT("EnemyTable.csv"), FSHMEnemyRow::StaticStruct(), this);
	if (!EnemyTable)
	{
		UE_LOG(LogSHMEncounter, Error, TEXT("敌人表加载失败——遭遇系统无法刷怪"));
		return;
	}

	EnemyTable->ForeachRow<FSHMEnemyRow>(TEXT("EncounterInit"),
		[this](const FName&, const FSHMEnemyRow& Row)
	{
		if (Row.ArchetypeTag.IsValid())
		{
			RowsByArchetype.Add(Row.ArchetypeTag, Row);
			ThreatByArchetype.Add(Row.ArchetypeTag, Row.ThreatValue);
		}
	});

	Rand.Initialize(FPlatformTime::Cycles());
}

int32 USHMEncounterManager::SpawnRoomWave(const TMap<FGameplayTag, float>& Weights, int32 ThreatBudget, const FVector& Center)
{
	if (RowsByArchetype.Num() == 0)
	{
		return 0;
	}

	const TArray<FGameplayTag> Wave =
		FSHMEncounterMath::BuildWave(Weights, ThreatByArchetype, ThreatBudget, Rand);

	RoomCenterZ = Center.Z;

	int32 Spawned = 0;
	for (const FGameplayTag& Archetype : Wave)
	{
		FVector Loc;
		if (!FindNavigableSpawnPoint(Center, Loc))
		{
			UE_LOG(LogSHMEncounter, Warning,
				TEXT("原型 %s 找不到可导航刷怪点（玩家周围导航网格太小？），本只跳过"),
				*Archetype.ToString());
			continue;
		}

		if (ASHMEnemy* Enemy = SpawnEnemyOfArchetype(Archetype, Loc))
		{
			AliveEnemies.Add(Enemy);
			++Spawned;
		}
	}

	if (Spawned > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(PollTimer, this,
			&USHMEncounterManager::PollRoomState, 0.5f, true);
	}

	UE_LOG(LogSHMEncounter, Log, TEXT("刷怪 %d 只（预算 %d）"), Spawned, ThreatBudget);
	return Spawned;
}

ASHMEnemy* USHMEncounterManager::SpawnEnemyOfArchetype(const FGameplayTag& ArchetypeTag, const FVector& Location)
{
	const FSHMEnemyRow* Row = RowsByArchetype.Find(ArchetypeTag);
	if (!Row)
	{
		UE_LOG(LogSHMEncounter, Warning, TEXT("原型 %s 无数据行，跳过"), *ArchetypeTag.ToString());
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ASHMEnemy* Enemy = GetWorld()->SpawnActor<ASHMEnemy>(
		ASHMEnemy::StaticClass(), Location + FVector(0, 0, 100.f), FRotator::ZeroRotator, Params);
	if (Enemy)
	{
		Enemy->InitFromRow(*Row);
	}
	return Enemy;
}

int32 USHMEncounterManager::GetAliveCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<ASHMEnemy>& Enemy : AliveEnemies)
	{
		if (IsValid(Enemy))
		{
			const USHMAttributeComponent* Attr = Enemy->AttributeComp;
			if (Attr && !Attr->IsDead())
			{
				++Count;
			}
		}
	}
	return Count;
}

bool USHMEncounterManager::FindNavigableSpawnPoint(const FVector& Center, FVector& OutLocation)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	// 由远及近尝试：优先远处（给玩家反应时间），平台小就逐步收缩到近处
	constexpr int32 MaxAttempts = 10;
	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const float Shrink = 1.f - (static_cast<float>(Attempt) / MaxAttempts) * 0.7f;  // 1.0 → 0.3
		const float Angle  = Rand.FRand() * 2.f * PI;
		const float Radius = Rand.FRandRange(700.f, 1000.f) * Shrink;
		const FVector Candidate = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

		// 投影到导航网格：垂直容差放宽（候选点悬在平台上方/下方一点都能吸附回来）
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Candidate, Projected, FVector(150.f, 150.f, 500.f)))
		{
			OutLocation = Projected.Location;
			return true;
		}
	}
	return false;
}

void USHMEncounterManager::PollRoomState()
{
	// 坠落清理：被击退坠台/物理异常掉出平台的怪永远追不到玩家也死不了，
	// 留着就是死锁。低于本房参考高度一定幅度直接清掉，视为非战斗减员。
	for (TObjectPtr<ASHMEnemy>& Enemy : AliveEnemies)
	{
		if (IsValid(Enemy) && Enemy->GetActorLocation().Z < RoomCenterZ - 600.f)
		{
			UE_LOG(LogSHMEncounter, Warning, TEXT("%s 坠出平台，清理（非战斗减员）"), *Enemy->GetName());
			Enemy->Destroy();
		}
	}

	if (GetAliveCount() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(PollTimer);
		AliveEnemies.Empty();
		OnRoomCleared.Broadcast();
	}
}
