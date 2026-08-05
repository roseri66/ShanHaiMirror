package com.shanhai.director.cache;

import java.util.List;

import com.shanhai.director.api.DirectorIntent;

/**
 * 缓存的存储后端。
 *
 * <p><b>这个接口存在的唯一理由是让 Redis 日后能真的插上</b>，而不是嘴上说说。
 * 指纹计算与轮询策略（{@link IntentCache}）不依赖具体存储，
 * 换后端只需要再写一个实现类，业务逻辑与它的测试一行不用改。
 *
 * <p>D-23 的技术选型原本写的是 Redis（spring-data-redis）。实际先用内存实现，
 * 原因是本机没有 Docker、内存也吃紧（详见 DECISIONS D-23 的偏离注）。
 * <b>缓存这件事真正值钱的是指纹分桶策略与"缓存会让台词重复"那个权衡，
 * 不是选哪个 KV 存储</b>——那些逻辑在内存实现上一样能写、一样能测。
 *
 * <p>换 Redis 时要注意两点，写在这里免得日后忘：
 * <ul>
 *   <li>{@link #append} 需要是原子的，否则并发下会超出 maxVariants</li>
 *   <li>Redis 有 TTL，内存实现没有——换过去后 {@code evictIfLarge} 那套可以删</li>
 * </ul>
 */
public interface IntentCacheStore {

    /** 取某个指纹下已缓存的全部候选。无缓存时返回空列表，不返回 null。 */
    List<DirectorIntent> get(String fingerprint);

    /**
     * 追加一条候选。
     *
     * @param maxVariants 该指纹最多保留几条；已达上限时应当忽略本次追加
     */
    void append(String fingerprint, DirectorIntent intent, int maxVariants);

    /** 当前缓存的指纹数量，供指标与测试使用。 */
    int size();

    /** 清空。测试用；生产上由各自的淘汰策略负责。 */
    void clear();
}
