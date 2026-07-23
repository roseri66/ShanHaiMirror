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

	int32 Spawned = 0;
	for (const FGameplayTag& Archetype : Wave)
	{
		// 环形随机点：远到玩家有反应时间，近到立刻构成威胁
		const float Angle  = Rand.FRand() * 2.f * PI;
		const float Radius = Rand.FRandRange(700.f, 1000.f);
		const FVector Loc  = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

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

void USHMEncounterManager::PollRoomState()
{
	if (GetAliveCount() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(PollTimer);
		AliveEnemies.Empty();
		OnRoomCleared.Broadcast();
	}
}
