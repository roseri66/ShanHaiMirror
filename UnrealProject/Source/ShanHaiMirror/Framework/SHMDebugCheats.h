// Copyright ShanHaiMirror. MIT License.
#pragma once

#include "CoreMinimal.h"

/**
 * 调试作弊开关（决策 D-25）。
 *
 * 存在的唯一理由是 **M5-5 的采样效率**：要校准指纹分桶，需要横跨值域的画像数据，
 * 而真实游玩太慢（远程 DPS 只有近战的 40%），靠硬打凑不出足够多样的样本。
 *
 * ## 为什么是控制台变量而不是改武器数值常量
 *
 * 改常量要靠「记得改回来」。控制台变量**默认 1.0 即不生效**，
 * 忘了关最多下次启动时自己没了——**不依赖记性的方案才算方案**。
 *
 * ## ⚠️ 这里刻意没有「敌人伤害」开关
 *
 * 把敌人伤害降到 1 会让血量永远跌不破 `LowHpThreshold`，
 * 于是 `LowHpEvents` 恒为 0、`SurvivalPressure` 恒为 0 ——
 * **而那正是这次采样最需要动起来的那一维**。
 * 想要「打得快」应该走高攻（{@link PlayerDamageMult}），
 * 想要「有生存压力」应该走脆皮（{@link PlayerMaxHPMult} < 1），
 * **而不是让敌人打不动人**。
 *
 * ## ⚠️ 作弊状态下采到的数据有效力边界
 *
 * 作弊会抬高战斗效率的速度分，所以这批数据能回答**结构问题**
 * （「去掉某字段会不会合并指纹」），**不能**回答**比率问题**
 * （「真实游玩的命中率是多少」）。
 * 故 {@link IsAnyActive} 会被决策请求带到服务端并落进 `debug_flags` 列 ——
 * **让这条边界由机器强制，而不是靠记性。**
 */
struct SHANHAIMIRROR_API FSHMDebugCheats
{
	/** 玩家武器伤害倍率。>1 让战斗变短。 */
	static float PlayerDamageMult();

	/** 玩家最大血量倍率。<1 让挨一下就残血，从而真的触发 LowHp 事件。 */
	static float PlayerMaxHPMult();

	/** 远程武器额外伤害倍率，叠加在 {@link PlayerDamageMult} 之上。 */
	static float RangedDamageMult();

	/** 是否有任一开关偏离了 1.0。决定请求要不要带调试标记。 */
	static bool IsAnyActive();

	/**
	 * 生效中的开关摘要，形如 {@code "dmg=4.0,hp=0.4,ranged=2.0"}；都没开时返回空串。
	 *
	 * <p>存进服务端的 `debug_flags` 列 —— 只记「开了没有」不够，
	 * **倍率不同的两批数据同样不可比**。
	 */
	static FString DescribeActive();
};
