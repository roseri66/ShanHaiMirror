#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SHMAIController.generated.h"

// ============================================================================
// ASHMAIController —— 通用敌人 AI Controller（Sprint 1：直接追玩家，不用行为树）
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API ASHMAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASHMAIController();

	virtual void Tick(float DeltaTime) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float AcceptanceRadius = 150.f;
};
