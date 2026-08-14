// Copyright ShanHaiMirror. MIT License.
#include "SHMDebugCheats.h"

#include "HAL/IConsoleManager.h"

namespace
{
	// 三个开关默认全是 1.0 —— 即「不生效」。见 D-25：默认不生效是这套方案成立的前提。
	TAutoConsoleVariable<float> CVarPlayerDamageMult(
		TEXT("shm.Debug.PlayerDamageMult"),
		1.0f,
		TEXT("玩家武器伤害倍率（D-25，仅用于 M5 采样）。1.0 = 不生效。\n")
		TEXT("例：shm.Debug.PlayerDamageMult 4"),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarPlayerMaxHPMult(
		TEXT("shm.Debug.PlayerMaxHPMult"),
		1.0f,
		TEXT("玩家最大血量倍率（D-25）。1.0 = 不生效。\n")
		TEXT("⚠️ 想要生存压力用这个调低血量，不要去削敌人伤害——\n")
		TEXT("   敌人打不动人会让 SurvivalPressure 恒为 0，正好废掉要采的那一维。"),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarRangedDamageMult(
		TEXT("shm.Debug.RangedDamageMult"),
		1.0f,
		TEXT("远程武器额外伤害倍率（D-25），叠加在 PlayerDamageMult 之上。\n")
		TEXT("实测远程 DPS 只有近战的 40%（12/0.6 对 20/0.4），采远程样本时用。"),
		ECVF_Cheat);

	/** 倍率的合法区间。0 或负数会让伤害归零/回血，那不是作弊是 bug。 */
	float SanitizeMult(float Value)
	{
		return FMath::Clamp(Value, 0.01f, 1000.f);
	}

	bool IsNeutral(float Value)
	{
		return FMath::IsNearlyEqual(Value, 1.0f);
	}
}

float FSHMDebugCheats::PlayerDamageMult()
{
	return SanitizeMult(CVarPlayerDamageMult.GetValueOnAnyThread());
}

float FSHMDebugCheats::PlayerMaxHPMult()
{
	return SanitizeMult(CVarPlayerMaxHPMult.GetValueOnAnyThread());
}

float FSHMDebugCheats::RangedDamageMult()
{
	return SanitizeMult(CVarRangedDamageMult.GetValueOnAnyThread());
}

bool FSHMDebugCheats::IsAnyActive()
{
	return !IsNeutral(PlayerDamageMult())
		|| !IsNeutral(PlayerMaxHPMult())
		|| !IsNeutral(RangedDamageMult());
}

FString FSHMDebugCheats::DescribeActive()
{
	if (!IsAnyActive())
	{
		return FString();
	}

	TArray<FString> Parts;
	if (!IsNeutral(PlayerDamageMult()))
	{
		Parts.Add(FString::Printf(TEXT("dmg=%.2f"), PlayerDamageMult()));
	}
	if (!IsNeutral(PlayerMaxHPMult()))
	{
		Parts.Add(FString::Printf(TEXT("hp=%.2f"), PlayerMaxHPMult()));
	}
	if (!IsNeutral(RangedDamageMult()))
	{
		Parts.Add(FString::Printf(TEXT("ranged=%.2f"), RangedDamageMult()));
	}
	return FString::Join(Parts, TEXT(","));
}
