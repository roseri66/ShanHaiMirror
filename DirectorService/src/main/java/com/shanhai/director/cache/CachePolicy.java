package com.shanhai.director.cache;

import java.util.function.LongSupplier;

/**
 * 「查缓存该返回什么」这条规则本身。
 *
 * <h2>⭐ 它为什么要单独存在</h2>
 *
 * 这条规则有两个使用者：
 * <ul>
 *   <li><b>线上</b> {@link IntentCache} —— 状态存在 {@code IntentCacheStore} 里</li>
 *   <li><b>离线</b> {@code CacheSimulator} —— 状态存在一个内存 Map 里，用来重放历史（M5，D-24）</li>
 * </ul>
 *
 * 两者的<b>状态存储</b>不同，但<b>判定规则必须完全一致</b> ——
 * 只要有一点分歧，模拟出来的命中率就和真实的不可比，而那正是模拟器唯一的用处。
 *
 * <p>本项目已经为「逻辑双写必然漂移」付过一次代价的判断（D-23 否决护栏上服务端，
 * 理由之一就是 C++/Java 双写对「连续」的定义只要差半点，表现就是玩家侧与统计侧对不上）。
 * <b>所以这里不留第二份实现</b>：规则抽成一个纯函数，两边各自管自己的状态。
 *
 * @since M5
 */
public final class CachePolicy {

    private CachePolicy() {
    }

    /**
     * 判定一次查询的结果。
     *
     * <h2>三条分支的由来</h2>
     *
     * <ol>
     *   <li><b>一条候选都没有</b> → 只能走 LLM</li>
     *   <li><b>已攒满</b> → 稳态，命中</li>
     *   <li><b>⭐ 预热期：边用边攒</b> → 有候选就能用，但<b>隔一次仍走 LLM 补充</b>
     *       <p>最初的规则是「未满一律不命中」，在真实流量下等于缓存永不生效：
     *       一局只发 2 次决策请求，而 F1/F2 天然是两条指纹 ——
     *       同一条指纹要攒满得连打 4 局以上，且期间画像分桶不能漂。
     *       <b>用户实测打了 3 把，一次都没命中。</b>
     *       <p>取舍是明的：<b>一个永远不命中的缓存，比偶尔重复一句台词糟得多。</b></li>
     * </ol>
     *
     * @param variantCount  该指纹下当前有几条候选
     * @param maxVariants   攒满的阈值
     * @param warmupCounter <b>只在预热分支被调用</b>，返回这条指纹的第几次预热期查询。
     *                      设计成回调而不是入参，是为了让「计数只在预热期递增」这条
     *                      由本方法保证，而不是让每个调用方各自记得
     * @return 本次查询的结果
     */
    public static CacheOutcome decide(int variantCount, int maxVariants, LongSupplier warmupCounter) {
        if (variantCount <= 0) {
            return CacheOutcome.MISS_EMPTY;
        }
        if (variantCount >= maxVariants) {
            return CacheOutcome.HIT;
        }
        return warmupCounter.getAsLong() % 2 == 0 ? CacheOutcome.HIT : CacheOutcome.MISS_WARMUP;
    }
}
