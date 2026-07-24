#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SHMEnemyTable.generated.h"

// ============================================================================
// DT_Enemy 行结构 —— 数据源 Data/EnemyTable.csv
// 四原型只是四行数值 + 一个颜色：行为差异由 AIController 按原型标签分支，
// 外观差异由运行时染色实现——加一种怪 = 加一行，不新建类不新建蓝图
// ============================================================================
USTRUCT(BlueprintType)
struct FSHMEnemyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag ArchetypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float HP = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Speed = 400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ContactDamage = 10.f;

	// 遭遇预算用：越强的怪占越多预算
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ThreatValue = 5;

	// Shooter=射程；近战原型=接触判定距离
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackCooldown = 1.2f;

	// 原型识别色（占位美术：形状同款，靠颜色 0.3 秒内分辨"这是什么怪"）
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::Gray;
};
