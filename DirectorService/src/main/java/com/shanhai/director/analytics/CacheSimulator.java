package com.shanhai.director.analytics;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.springframework.stereotype.Component;

import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.CachePolicy;
import com.shanhai.director.cache.FingerprintScheme;
import com.shanhai.director.persistence.IntentRecord;

/**
 * 在历史流水上重放缓存，回答<b>「换一套指纹方案会怎样」</b>（M5，决策 D-24）。
 *
 * <h2>它为什么存在</h2>
 *
 * M3 定下的两个参数（桶宽 20 分、候选 3 条）当时是<b>拍的</b>，
 * 代码注释里写着「待回流数据校准」。M5 就是来还这笔债的。
 *
 * <p>但校准的前提是能<b>比较</b>：光知道「当前方案命中率 34%」没有用，
 * 要知道「桶宽改成 50 会变成多少」才能决定改不改。
 * 而这只能靠在同一批历史请求上重放不同的方案。
 *
 * <h2>⭐ 正确性锚点</h2>
 *
 * 一个模拟器最容易犯的错是<b>算出一个看起来合理但不对的数</b> ——
 * 而它有一个天然的验证方式：<b>拿它去预测已经发生过的事。</b>
 *
 * <p>用 {@link FingerprintScheme#CURRENT} 跑出来的结果，<b>必须逐条等于
 * 数据库里记着的真实 {@code cache_outcome}</b>。不等就是模拟器写错了。
 * 这条写成了测试（{@code CacheSimulatorTest.currentScheme_reproducesRecordedHistory}）。
 *
 * <p>这正是本项目「别用自己编的流量验证效果类功能」那条教训的正面应用：
 * 当初缓存单测 11 条全绿、连发 12 次完美命中，而真实打三局一次都没命中 ——
 * <b>因为验证用的是一个可控的替代问题，而替代问题总是更容易通过。</b>
 *
 * <h2>⚠️ 一个必须说清的建模假设</h2>
 *
 * 重放时要判断「这次未命中之后，会不会往缓存里放一条」。真实历史里这取决于
 * LLM 当时成不成功，而<b>换了方案之后，原本命中的请求可能变成未命中</b> ——
 * 那一次 LLM 从没被调用过，它会不会成功<b>无从得知</b>。
 *
 * <p>本模拟器的假设是：<b>原本走到 {@code Llm} 或 {@code Cache} 的请求，
 * 视为「LLM 可用且会成功」</b>；{@code Upstream503}（调了但失败）和
 * {@code ServerLocal}（没配 key）视为不会产生可缓存的结果。
 *
 * <p><b>这个假设会让模拟结果偏乐观</b>（真实世界里 LLM 有失败率）。
 * 写在这里而不是藏起来 —— 一个不说明假设的模拟结果，和拍脑袋没有区别。
 *
 * @since M5
 */
@Component
public class CacheSimulator {

    /** 原始请求的 source 里，哪些意味着「LLM 这条路当时是通的」。见类注释的建模假设。 */
    private static final Set<String> LLM_PATH_VIABLE = Set.of("Llm", "Cache");

    /**
     * 在同一批历史流水上跑多套方案。
     *
     * @param records <b>必须按时间升序</b> —— 缓存状态的演进是有顺序的，顺序错了结果没有意义
     * @param schemes 要对照的方案
     */
    public List<SchemeResult> simulate(List<IntentRecord> records, List<FingerprintScheme> schemes) {
        List<SchemeResult> results = new ArrayList<>(schemes.size());
        for (FingerprintScheme scheme : schemes) {
            results.add(runOne(records, scheme));
        }
        return results;
    }

    /**
     * 指纹里的每个字段，各自把样本切成了多碎。
     *
     * <h2>⭐ 它要回答的问题</h2>
     *
     * 读代码时发现两件待验证的事（记在 D-24 里）：
     * <ol>
     *   <li>{@code challengeBudget} 疑似被 {@code floorIndex} 完全决定 ——
     *       客户端的 {@code ChallengeBudgetForFloor} 是 {@code FloorIndex} 的纯函数。
     *       <b>但服务端收到的是客户端传来的值，不该假设它一定是那个公式</b>，
     *       所以这是一个假设，要由数据来验证</li>
     *   <li>{@code floorIndex} 把一局的两次请求强制切成两条指纹</li>
     * </ol>
     *
     * <p>本方法把这两个假设变成<b>数据自己说出来的结论</b>：
     * 去掉某个字段后指纹数完全不变 ⇒ 它被别的字段完全决定，是冗余的。
     *
     * @param records 历史流水
     * @param base    以哪套方案为基准做对照
     */
    public List<FieldContribution> analyzeFieldContribution(List<IntentRecord> records,
                                                            FingerprintScheme base) {
        int baseDistinct = countDistinct(records, base);

        List<FieldContribution> out = new ArrayList<>();
        for (FingerprintScheme.Field field : base.fields()) {
            // 只剩一个字段时去掉它会构造出空方案（构造期会抛），跳过
            if (base.fields().size() <= 1) {
                break;
            }
            FingerprintScheme without = base.without(field);
            int withoutDistinct = countDistinct(records, without);

            // 这个字段本身有几种取值（去掉别人只留它）
            int ownValues = countDistinct(records,
                    new FingerprintScheme("only-" + field, base.profileBucket(),
                            base.maxVariants(), java.util.EnumSet.of(field)));

            out.add(new FieldContribution(
                    field.name(),
                    ownValues,
                    withoutDistinct,
                    withoutDistinct == 0 ? 1.0 : (double) baseDistinct / withoutDistinct,
                    withoutDistinct == baseDistinct));
        }
        out.sort((a, b) -> Double.compare(b.splitFactor(), a.splitFactor()));
        return out;
    }

    /**
     * 找出<b>一一对应</b>的字段对。
     *
     * <h2>⭐ 它为什么必须存在</h2>
     *
     * {@link #analyzeFieldContribution} 只能测<b>边际贡献</b>：去掉这一个字段，指纹数变不变。
     * <b>而边际贡献为零不等于冗余。</b>
     *
     * <p>实测暴露了这个缺陷：一批 6 局 × 2 层的数据跑出来，
     * <b>八个字段里七个的边际贡献都是零</b> —— 包括 {@code PROFILE}。
     * 原因是字段之间高度相关，去掉任意<b>一个</b>，剩下的组合仍然能把样本分开。
     *
     * <p>典型的就是 {@code FLOOR} 与 {@code BUDGET}：<b>它们互为替代</b>，
     * 去掉任一个都不影响，但两个都去掉就会合并。单字段分析永远说不清这件事。
     *
     * <p>本方法直接检验<b>函数上的一一对应</b>：
     * 若 {@code |A| == |B| == |{A,B}|}，则 A 的取值与 B 的取值之间存在双射 ——
     * 也就是说<b>知道其中一个就知道另一个，留两个纯属白占</b>。
     *
     * <p>这才是 D-24 里假设①（{@code challengeBudget} 被 {@code floorIndex} 完全决定）
     * 的准确检验方式，而且它<b>不依赖其它字段</b>，因此在小样本上也稳定。
     */
    public List<EquivalentPair> findEquivalentPairs(List<IntentRecord> records,
                                                     FingerprintScheme base) {
        List<FingerprintScheme.Field> fields = new ArrayList<>(base.fields());
        Map<FingerprintScheme.Field, Integer> ownCounts = new HashMap<>();
        for (FingerprintScheme.Field f : fields) {
            ownCounts.put(f, countDistinct(records, single(base, f)));
        }

        List<EquivalentPair> pairs = new ArrayList<>();
        for (int i = 0; i < fields.size(); i++) {
            for (int j = i + 1; j < fields.size(); j++) {
                FingerprintScheme.Field a = fields.get(i);
                FingerprintScheme.Field b = fields.get(j);
                int ca = ownCounts.get(a);
                int cb = ownCounts.get(b);
                if (ca != cb) {
                    continue;
                }
                int combined = countDistinct(records, new FingerprintScheme(
                        "pair", base.profileBucket(), base.maxVariants(),
                        java.util.EnumSet.of(a, b)));
                if (combined == ca) {
                    // ⚠️ 取值只有 1 种时任意两个字段都"一一对应"，那是样本太单一，不是发现。
                    //    照实标出来，别让一个空洞的结论看起来像个洞察。
                    pairs.add(new EquivalentPair(a.name(), b.name(), ca, ca <= 1));
                }
            }
        }
        return pairs;
    }

    private FingerprintScheme single(FingerprintScheme base, FingerprintScheme.Field f) {
        return new FingerprintScheme("only-" + f, base.profileBucket(),
                base.maxVariants(), java.util.EnumSet.of(f));
    }

    /**
     * 两个在样本里一一对应的字段。
     *
     * @param fieldA        字段 A
     * @param fieldB        字段 B
     * @param distinctValues 二者各自的取值种数（相等才可能一一对应）
     * @param trivial       <b>⚠️ 取值只有 1 种</b> —— 此时任意两字段都"一一对应"，
     *                      这是样本太单一，不是发现
     */
    public record EquivalentPair(
            String fieldA,
            String fieldB,
            int distinctValues,
            boolean trivial) {
    }

    private int countDistinct(List<IntentRecord> records, FingerprintScheme scheme) {
        Set<String> seen = new HashSet<>();
        for (IntentRecord r : records) {
            seen.add(scheme.compute(r.input()));
        }
        return seen.size();
    }

    /**
     * 单个字段对「指纹被切多碎」的贡献。
     *
     * @param field            字段名
     * @param distinctValues   这个字段本身在样本里有几种取值
     * @param distinctWithout  把它从指纹里去掉后，还剩多少条不同的指纹
     * @param splitFactor      去掉它之后指纹数缩小的倍数。<b>越大说明它切得越碎</b>
     * @param marginallyRedundant
     *        <b>⚠️ 单独去掉它，指纹数不变。</b>
     *        <p>这<b>不等于</b>「它是冗余的」—— 只说明在其余字段都在的前提下，
     *        它不带来额外区分度。字段之间高度相关时，
     *        <b>可能每一个的边际贡献都是零，而它们合起来是必需的。</b>
     *        <p>实测见过八个字段里七个都是 true 的情况。
     *        真正要判断「谁可以去掉」，看 {@link EquivalentPair}。
     */
    public record FieldContribution(
            String field,
            int distinctValues,
            int distinctWithout,
            double splitFactor,
            boolean marginallyRedundant) {
    }

    private SchemeResult runOne(List<IntentRecord> records, FingerprintScheme scheme) {
        // 每套方案从空缓存开始重放，互不影响
        Map<String, Integer> variantCounts = new HashMap<>();
        Map<String, Long> warmupSeq = new HashMap<>();
        Map<CacheOutcome, Integer> breakdown = new HashMap<>();
        Set<String> distinct = new HashSet<>();

        List<CacheOutcome> trace = new ArrayList<>(records.size());

        for (IntentRecord record : records) {
            String fp = scheme.compute(record.input());
            distinct.add(fp);

            int variants = variantCounts.getOrDefault(fp, 0);

            // ⭐ 用与线上完全相同的判定规则（CachePolicy），只是状态存在本地 Map 里。
            //    规则不在这里重写 —— 重写就是「逻辑双写必然漂移」，
            //    而一旦漂移，模拟出来的命中率就和真实的不可比。
            CacheOutcome outcome = CachePolicy.decide(variants, scheme.maxVariants(),
                    () -> warmupSeq.merge(fp, 1L, Long::sum));

            breakdown.merge(outcome, 1, Integer::sum);
            trace.add(outcome);

            // 未命中且 LLM 这条路当时是通的 → 模拟一次 store
            if (!outcome.isHit() && LLM_PATH_VIABLE.contains(record.source())) {
                variantCounts.put(fp, Math.min(scheme.maxVariants(), variants + 1));
            }
        }

        int total = records.size();
        int hits = breakdown.getOrDefault(CacheOutcome.HIT, 0);
        return new SchemeResult(
                scheme.name(),
                scheme.profileBucket(),
                scheme.maxVariants(),
                total,
                distinct.size(),
                hits,
                total == 0 ? 0.0 : (double) hits / total,
                total == 0 ? 0.0 : (double) total / distinct.size(),
                breakdown.getOrDefault(CacheOutcome.MISS_EMPTY, 0),
                breakdown.getOrDefault(CacheOutcome.MISS_WARMUP, 0),
                trace);
    }

    /**
     * 一套方案在历史流水上的表现。
     *
     * @param schemeName            方案名
     * @param profileBucket         桶宽
     * @param maxVariants           候选数
     * @param totalRequests         样本量。<b>⚠️ 看结论前先看它</b> —— 见 {@link #sampleWarning()}
     * @param distinctFingerprints  产生了多少条不同的指纹。<b>越少说明合并得越狠</b>
     * @param hits                  命中次数
     * @param hitRate               命中率
     * @param avgRequestsPerFingerprint 平均每条指纹被请求几次。
     *                              <b>⭐ 它小于 maxVariants 就说明「候选攒不满」是必然的，
     *                              而不是运气问题</b>
     * @param missEmpty             因「这条指纹从没见过」而未命中的次数
     * @param missWarmup            因「候选还没攒满、主动去补」而未命中的次数
     * @param trace                 逐条的判定结果，供正确性锚点比对
     */
    public record SchemeResult(
            String schemeName,
            int profileBucket,
            int maxVariants,
            int totalRequests,
            int distinctFingerprints,
            int hits,
            double hitRate,
            double avgRequestsPerFingerprint,
            int missEmpty,
            int missWarmup,
            List<CacheOutcome> trace) {

        /**
         * 样本量够不够得出结论。
         *
         * <p>⚠️ 这个方法存在的理由：<b>拿十几条数据说「我校准了参数」，
         * 正是本项目在演示数据那次犯过的错</b>（构造出产品跑不出来的状态，
         * 用户审阅时点破了它）。所以宁可让每一份报告都带上这句提醒。
         */
        public String sampleWarning() {
            if (totalRequests < 30) {
                return "⚠️ 样本量仅 " + totalRequests + " 条，不足以支撑参数决策。"
                        + "此结果只能用于看趋势，不能作为改动依据。";
            }
            if (totalRequests < 100) {
                return "⚠️ 样本量 " + totalRequests + " 条，偏小。结论请标注样本量。";
            }
            return null;
        }
    }
}
