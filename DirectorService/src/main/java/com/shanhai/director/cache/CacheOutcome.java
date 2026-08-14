package com.shanhai.director.cache;

/**
 * 一次缓存查询的结果类型。
 *
 * <h2>为什么要区分两种「未命中」</h2>
 *
 * 从调用方看，{@link #MISS_EMPTY} 和 {@link #MISS_WARMUP} 都是「去走 LLM」，
 * 行为完全一样。但它们的<b>含义</b>完全不同：
 *
 * <ul>
 *   <li>{@link #MISS_EMPTY} —— 这条指纹<b>从来没见过</b>。多了说明指纹切得太碎</li>
 *   <li>{@link #MISS_WARMUP} —— 见过，只是候选还没攒满，<b>这是设计内的主动补充</b></li>
 * </ul>
 *
 * <p><b>把两者混成一个「未命中」，就无法区分「缓存方案有问题」和「缓存还在预热」</b> ——
 * 而这正是 M5（决策 D-24）要回答的问题。所以从落库的第一天起就分开记。
 *
 * <p>这与本项目给 429 记独立日志是同一条判断：<b>两件事的应对方式不同，
 * 混在一起记就等于都查不出来。</b>
 *
 * @since M5
 */
public enum CacheOutcome {

    /** 命中：该指纹下有候选，随机取了一条。 */
    HIT,

    /** 未命中：该指纹一条候选都没有。 */
    MISS_EMPTY,

    /** 未命中：该指纹有候选但未攒满，按「隔一次补一条」的规则主动去 LLM 补充。 */
    MISS_WARMUP;

    public boolean isHit() {
        return this == HIT;
    }
}
