package com.shanhai.director.cache;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicLong;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import com.shanhai.director.api.DirectorIntent;
import com.shanhai.director.api.IntentRequest;

/**
 * 决策缓存（M3）。
 *
 * <h2>指纹怎么算</h2>
 *
 * 直接拿请求体当 key 等于永不命中：{@code runId} 每局都变、浮点画像每次都不同。
 * 指纹只取<b>影响决策的量</b>，且对连续量<b>分桶</b>：
 *
 * <table border="1">
 *   <tr><th>入 key</th><th>处理</th><th>为什么</th></tr>
 *   <tr><td>floorIndex / challengeBudget</td><td>原值</td><td>离散，且直接影响预算护栏</td></tr>
 *   <tr><td>availableRules / Archetypes</td><td><b>排序后</b>拼接</td><td>集合语义，顺序无意义</td></tr>
 *   <tr><td>五维画像</td><td><b>每 20 分一桶</b></td><td>87 分和 85 分不该算两种玩家</td></tr>
 *   <tr><td>confidence</td><td><b>三档</b></td><td>护栏只在 0.6 处有阈值行为，分更细是假精度</td></tr>
 *   <tr><td>dominantArchetype</td><td>原值</td><td>离散</td></tr>
 *   <tr><td>decisionHistory</td><td>只取 tag 列表</td><td>Fairness 只关心"用过没有"</td></tr>
 *   <tr><td><b>不入 key</b></td><td>runId · 具体浮点</td><td>与决策无关，入了必不命中</td></tr>
 * </table>
 *
 * <h2>缓存会让台词重复 —— 这是真问题，不掩盖</h2>
 *
 * {@code narration} 恰恰是最该每次都不一样的字段。缓存整个 Intent，
 * 命中率上去了，代价是<b>同类画像的玩家听到同一句台词</b>，
 * 而白泽的人格是这个项目的体验核心之一。
 *
 * <p>解法：同一指纹下缓存<b>最多 3 条</b>，命中时随机取一条；未满 3 条时
 * 仍走 LLM 并追加。稳态命中率约 2/3、成本降到 1/3，同类玩家听到三种说法。
 * 三条的结构决策（ruleIntents）大概率一致，这没关系——结构本来就该稳定，
 * 该变的是表达。
 *
 * <p><b>桶大小（20 分）与候选条数（3）都是拍的，不是测出来的。</b>
 * 上线后靠回流数据回头调——这正是"数据回流"那条目标的第一个实际用途，
 * 而不是一句空口号。现在把它们写成常量并在这里注明来源，
 * 是为了日后调整时知道该找什么依据，而不是又拍一次。
 */
@Component
public class IntentCache {

    private static final Logger log = LoggerFactory.getLogger(IntentCache.class);

    /**
     * 同一指纹保留几条候选。**拍的值**，待回流数据校准。
     *
     * <p>M5 起真源在 {@link FingerprintScheme#DEFAULT_MAX_VARIANTS}，这里只是转发——
     * 保留这个常量是因为既有测试引用了它，而改测试不属于本次范围。
     */
    public static final int MAX_VARIANTS = FingerprintScheme.DEFAULT_MAX_VARIANTS;

    /**
     * 线上正在用的指纹方案。
     *
     * <p>M5（决策 D-24）把「算指纹」从本类抽到了 {@link FingerprintScheme}，
     * 因为离线模拟器要用**同一份算法**跑不同的参数组合——各写一份必然漂移，
     * 而一旦漂移，模拟出来的命中率就和真实的不可比，那套东西也就失去了全部意义。
     * **本类的行为一字未改**，既有的缓存测试就是这条的守卫。
     */
    private static final FingerprintScheme SCHEME = FingerprintScheme.CURRENT;

    private final IntentCacheStore store;

    private final AtomicLong hits = new AtomicLong();
    private final AtomicLong misses = new AtomicLong();

    /**
     * 每条指纹的查询次数，用于预热期的"隔一次补一条"。
     *
     * <p>用计数而非随机：随机会让"打 3 局能省几次"变成概率事件，
     * 测试要么容易假绿要么容易 flaky。确定性的交替让收益可预测、可测。
     *
     * <p>与 store 一起被 evict：指纹清空后计数也该重来，
     * 否则残留的计数会让新指纹的第一次查询就走缓存分支。
     */
    private final Map<String, AtomicLong> lookupCounts = new ConcurrentHashMap<>();

    public IntentCache(IntentCacheStore store) {
        this.store = store;
    }

    /**
     * 查缓存。
     *
     * @return 命中时返回随机一条候选；未命中或候选未满 3 条时返回 empty（调用方应走 LLM）
     */
    public Optional<DirectorIntent> lookup(IntentRequest request) {
        String fp = fingerprint(request);
        List<DirectorIntent> variants = store.get(fp);

        // 一条都没有：只能走 LLM。
        if (variants.isEmpty()) {
            misses.incrementAndGet();
            log.debug("缓存未命中（无候选），走 LLM。指纹={}", fp);
            return Optional.empty();
        }

        // 已攒满：稳态，随机取一条。
        if (variants.size() >= MAX_VARIANTS) {
            return hit(variants, fp);
        }

        // ★ 预热期：**边用边攒**，隔一次去 LLM 补一条。
        //
        // 最初的规则是"未满 3 条一律不命中"，在真实流量下等于缓存永不生效：
        // 一局只发 2 次决策请求（共 3 层，F0 是观察层不走 Provider），
        // 而 F1/F2 的预算不同、天然是两条指纹——同一条指纹要攒满 3 次，
        // 得连打 4 局以上，且期间画像分桶不能漂。
        // 用户实测打了 3 把，**一次都没命中**。
        //
        // 现在改成：有候选就能用，但未满时隔一次仍走 LLM 补充。
        // 代价是预热期内同一句台词会重复出现一两次；
        // 收益是从第 2 次请求起就开始省钱，而且最终仍收敛到 3 条。
        // 这个取舍是明的：**一个永远不命中的缓存，比偶尔重复一句台词糟得多。**
        long n = lookupCounts.computeIfAbsent(fp, k -> new AtomicLong()).incrementAndGet();
        if (n % 2 == 0) {
            return hit(variants, fp);
        }

        misses.incrementAndGet();
        log.debug("预热期主动补充候选（{}/{}），走 LLM。指纹={}", variants.size(), MAX_VARIANTS, fp);
        return Optional.empty();
    }

    private Optional<DirectorIntent> hit(List<DirectorIntent> variants, String fp) {
        DirectorIntent picked = variants.get(ThreadLocalRandom.current().nextInt(variants.size()));
        hits.incrementAndGet();
        log.info("缓存命中（{} 选 1），指纹={}", variants.size(), fp);
        return Optional.of(picked);
    }

    /** 把一次真实 LLM 结果放进缓存。 */
    public void store(IntentRequest request, DirectorIntent intent) {
        store.append(fingerprint(request), intent, MAX_VARIANTS);
    }

    /** 某个语境下已攒到几条候选。测试与诊断用。 */
    public int variantCount(IntentRequest request) {
        return store.get(fingerprint(request)).size();
    }

    /** 命中率，供 M4 指标使用。无调用时返回 0。 */
    public double hitRatio() {
        long h = hits.get();
        long total = h + misses.get();
        return total == 0 ? 0.0 : (double) h / total;
    }

    public long hitCount() {
        return hits.get();
    }

    public long missCount() {
        return misses.get();
    }

    /**
     * 计算指纹。
     *
     * <p>包级可见是为了让测试能直接验它——指纹算错不会报错，
     * 只会表现为"命中率异常低"或"不该命中的命中了"，两者都极难在运行中发现。
     */
    String fingerprint(IntentRequest req) {
        return SCHEME.compute(FingerprintInput.from(req));
    }
}
