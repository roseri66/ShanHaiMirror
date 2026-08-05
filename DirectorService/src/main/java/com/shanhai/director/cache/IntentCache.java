package com.shanhai.director.cache;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
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

    /** 同一指纹保留几条候选。**拍的值**，待回流数据校准。 */
    public static final int MAX_VARIANTS = 3;

    /** 五维画像的分桶宽度（分）。**拍的值**，待回流数据校准。 */
    private static final int PROFILE_BUCKET = 20;

    /** 画像里参与指纹的五个维度。**不含 resourceSurplus**——它恒为 50，
     *  无数据源（D-09），入 key 只是给每条指纹加一个常量后缀，纯浪费。 */
    private static final List<String> PROFILE_DIMENSIONS = List.of(
            "buildConcentration", "combatEfficiency",
            "strategySwitch", "survivalPressure");

    private final IntentCacheStore store;

    private final AtomicLong hits = new AtomicLong();
    private final AtomicLong misses = new AtomicLong();

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

        // **未满 3 条时故意不命中**：先把三种说法攒齐，
        // 否则第一条会被反复命中，等于回到"千人一句"。
        if (variants.size() < MAX_VARIANTS) {
            misses.incrementAndGet();
            log.debug("缓存未命中（候选 {}/{}），走 LLM。指纹={}", variants.size(), MAX_VARIANTS, fp);
            return Optional.empty();
        }

        DirectorIntent picked = variants.get(ThreadLocalRandom.current().nextInt(variants.size()));
        hits.incrementAndGet();
        log.info("缓存命中（{} 选 1），指纹={}", variants.size(), fp);
        return Optional.of(picked);
    }

    /** 把一次真实 LLM 结果放进缓存。 */
    public void store(IntentRequest request, DirectorIntent intent) {
        store.append(fingerprint(request), intent, MAX_VARIANTS);
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
        StringBuilder sb = new StringBuilder();

        sb.append("f=").append(orZero(req.floorIndex()));
        sb.append("|b=").append(orZero(req.challengeBudget()));

        Map<String, Object> profile = req.profile();
        for (String dim : PROFILE_DIMENSIONS) {
            sb.append('|').append(dim, 0, 2).append('=').append(bucket(num(profile, dim)));
        }
        sb.append("|c=").append(confidenceTier(num(profile, "confidence")));
        sb.append("|a=").append(str(profile, "dominantArchetype"));

        // 集合语义：排序后拼接。不排序的话同一组候选换个顺序就是另一条指纹，
        // 命中率会莫名其妙地低，而且极难发现原因。
        sb.append("|r=").append(sortedJoin(ruleKeys(req)));
        sb.append("|e=").append(sortedJoin(req.availableArchetypes()));

        // 历史只取规则 tag：Fairness 只关心"这条规则用过没有"。
        // 带上层号的话每层都是新指纹，等于缓存失效。
        sb.append("|h=").append(sortedJoin(historyTags(req)));

        return sb.toString();
    }

    private static List<String> ruleKeys(IntentRequest req) {
        List<String> out = new ArrayList<>();
        if (req.availableRules() != null) {
            for (IntentRequest.AvailableRule r : req.availableRules()) {
                out.add(r.tag() + ":" + r.level());
            }
        }
        return out;
    }

    private static List<String> historyTags(IntentRequest req) {
        List<String> out = new ArrayList<>();
        if (req.decisionHistory() != null) {
            for (IntentRequest.HistoryEntry e : req.decisionHistory()) {
                if (e.ruleTags() != null) {
                    out.addAll(e.ruleTags());
                }
            }
        }
        return out;
    }

    private static String sortedJoin(List<String> items) {
        if (items == null || items.isEmpty()) {
            return "-";
        }
        List<String> copy = new ArrayList<>(items);
        copy.sort(String::compareTo);
        return String.join(",", copy);
    }

    /** 五维分桶：0-19 → 0，20-39 → 1，… 87 和 85 落进同一桶。 */
    private static int bucket(double v) {
        return (int) (Math.max(0, Math.min(100, v)) / PROFILE_BUCKET);
    }

    /**
     * 置信度三档。
     *
     * <p>护栏只在 0.6 处有阈值行为（低置信度禁重度规则），
     * 分得比这更细是假精度——0.71 和 0.79 对决策没有任何区别，
     * 却会让它们落进不同的缓存槽。
     */
    private static String confidenceTier(double c) {
        if (c < 0.6) {
            return "lo";
        }
        return c <= 0.8 ? "mid" : "hi";
    }

    private static int orZero(Integer v) {
        return v == null ? 0 : v;
    }

    private static double num(Map<String, Object> m, String key) {
        if (m == null) {
            return 0.0;
        }
        Object v = m.get(key);
        return v instanceof Number n ? n.doubleValue() : 0.0;
    }

    private static String str(Map<String, Object> m, String key) {
        if (m == null) {
            return "-";
        }
        Object v = m.get(key);
        return v instanceof String s && !s.isBlank() ? s : "-";
    }
}
