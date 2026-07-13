#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SMPlayerController.generated.h"

UCLASS()
class SHANHAIMIRROR_API ASMPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASMPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

protected:
	/** 鼠标点击移动 */
	void OnMoveToPressed();

	/** 键盘 WASD 移动 */
	void OnMoveX(float Value);
	void OnMoveY(float Value);

private:
	/** 俯视角摄像机高度 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraHeight = 1500.0f;

	/** 摄像机俯仰角度 */
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraPitch = -70.0f;

	void SetupTopDownCamera();
};
