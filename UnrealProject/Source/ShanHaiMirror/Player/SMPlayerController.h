#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SMPlayerController.generated.h"

// ============================================================================
// ASMPlayerController —— 最小化。只设置鼠标光标 + Enhanced Input 子系统。
// 摄像机在 ASMCharacter 上（SpringArm + Camera）。
// ============================================================================
UCLASS()
class SHANHAIMIRROR_API ASMPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASMPlayerController();

	virtual void BeginPlay() override;

	// 给 Character 提供鼠标世界坐标（攻击方向/瞄准用）
	UFUNCTION(BlueprintPure)
	bool GetMouseWorldLocation(FVector& OutLocation) const;
};
