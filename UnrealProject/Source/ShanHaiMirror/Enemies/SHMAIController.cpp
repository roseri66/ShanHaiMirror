#include "SHMAIController.h"
#include "SHMEnemy.h"
#include "Framework/SHMGameplayTags.h"
#include "Kismet/GameplayStatics.h"

ASHMAIController::ASHMAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASHMAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ASHMEnemy* Enemy  = Cast<ASHMEnemy>(GetPawn());
	if (!PlayerPawn || !Enemy)
	{
		return;
	}

	// 行为按原型标签分支——四原型共享一个 Controller，差异只在这里和数值。
	// （正经行为树是内容期的活，本阶段要的是克制关系可感，见第三次开工计划）
	if (Enemy->ArchetypeTag == SHMTags::Enemy_Shooter.GetTag())
	{
		// Shooter：保持距离射击，被近身则后撤
		const float Dist  = FVector::Dist2D(Enemy->GetActorLocation(), PlayerPawn->GetActorLocation());
		const float Range = Enemy->GetAttackRange();

		if (Dist > Range)
		{
			MoveToActor(PlayerPawn, Range * 0.8f);
		}
		else if (Dist < Range * 0.45f)
		{
			// 后撤：朝远离玩家的方向挪一段
			const FVector Away = (Enemy->GetActorLocation() - PlayerPawn->GetActorLocation()).GetSafeNormal2D();
			MoveToLocation(Enemy->GetActorLocation() + Away * 300.f, 50.f);
		}
		else
		{
			StopMovement();
			// 面向玩家再打
			FVector ToPlayer = PlayerPawn->GetActorLocation() - Enemy->GetActorLocation();
			ToPlayer.Z = 0.f;
			Enemy->SetActorRotation(ToPlayer.Rotation());
			Enemy->TryRangedAttack(PlayerPawn);
		}
	}
	else
	{
		// Grunt / Tank / Rush：追上去贴脸（差异全在数值：速度/血量/伤害）
		MoveToActor(PlayerPawn, AcceptanceRadius);
	}
}
