package com.shanhai.director.analytics;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import com.shanhai.director.api.IntentRequest;
import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.FingerprintInput;
import com.shanhai.director.cache.FingerprintScheme;
import com.shanhai.director.cache.IntentCache;
import com.shanhai.director.cache.InMemoryIntentCacheStore;
import com.shanhai.director.api.DirectorIntent;
import com.shanhai.director.persistence.IntentRecord;

/**
 * {@link CacheSimulator} 的测试（M5，决策 D-24）。
 *
 * <p>核心是第一条 —— <b>正确性锚点</b>。其余几条验的是「换方案确实会改变结果」。
 */
class CacheSimulatorTest {

    private final CacheSimulator simulator = new CacheSimulator();

    /**
     * ⭐⭐ 正确性锚点：<b>用 CURRENT 方案模拟，必须逐条重现真实发生过的历史。</b>
     *
     * <h2>为什么这条是整个模拟器的地基</h2>
     *
     * 一个模拟器最容易犯的错是<b>算出一个看起来合理但不对的数</b> ——
     * 而它恰好有一个天然的验证方式：<b>拿它去预测已经发生过的事。</b>
     *
     * <p>本用例的做法是：让<b>真的 {@link IntentCache}</b> 跑一遍请求序列，
     * 把每一次的真实 outcome 记下来（这就是数据库里会存的东西），
     * 然后让模拟器用同一套方案重放同一批记录 —— <b>两串 outcome 必须逐条相等。</b>
     *
     * <p>这正是本项目「别用自己编的流量验证效果类功能」那条教训的正面应用：
     * 当初缓存单测 11 条全绿、连发 12 次完美命中，而真实打三局一次都没命中 ——
     * 因为验证用的是一个可控的替代问题，<b>而替代问题总是更容易通过</b>。
     * 这里不再造替代问题，直接拿真实组件产生的真实序列当基准。
     */
    @Test
    @DisplayName("⭐ CURRENT 方案的模拟结果逐条等于真实 IntentCache 跑出来的历史")
    void currentScheme_reproducesRecordedHistory() {
        IntentCache realCache = new IntentCache(new InMemoryIntentCacheStore());

        // 造一段有代表性的序列：三条不同的指纹交错出现，覆盖
        // MISS_EMPTY（第一次见）→ MISS_WARMUP（预热期补充）→ HIT（攒满）三种结局
        List<IntentRequest> sequence = new ArrayList<>();
        for (int round = 0; round < 6; round++) {
            sequence.add(request("run-" + round, 1, 30, 87));   // 指纹 A
            sequence.add(request("run-" + round, 2, 55, 87));   // 指纹 B（层不同）
            sequence.add(request("run-" + round, 1, 30, 20));   // 指纹 C（画像不同桶）
        }

        List<CacheOutcome> realOutcomes = new ArrayList<>();
        List<IntentRecord> records = new ArrayList<>();
        Instant t = Instant.parse("2026-08-14T00:00:00Z");

        for (IntentRequest req : sequence) {
            IntentCache.LookupResult r = realCache.lookupDetailed(req);
            realOutcomes.add(r.outcome());

            // 真实控制器的行为：未命中才调 LLM 并 store
            String source = r.outcome().isHit() ? "Cache" : "Llm";
            if (!r.outcome().isHit()) {
                realCache.store(req, someIntent());
            }
            records.add(IntentRecord.of(req, r.fingerprint(), r.outcome(), r.variantCount(),
                    source, 200, 100L, t));
            t = t.plusSeconds(1);
        }

        CacheSimulator.SchemeResult simulated =
                simulator.simulate(records, List.of(FingerprintScheme.CURRENT)).get(0);

        assertThat(simulated.trace())
                .as("模拟器必须能重现已经发生过的事——不然它算出来的任何数字都不可信")
                .containsExactlyElementsOf(realOutcomes);

        // 顺带确认这段序列确实覆盖了三种结局，否则这条锚点是空的
        assertThat(realOutcomes).contains(
                CacheOutcome.MISS_EMPTY, CacheOutcome.MISS_WARMUP, CacheOutcome.HIT);
    }

    /**
     * ⭐ 验证 D-24 里记下的假设①：{@code challengeBudget} 是否被 {@code floorIndex} 完全决定。
     *
     * <p>本用例用的是<b>符合客户端公式</b>的数据（F1→30、F2→55），
     * 所以去掉 budget 之后指纹数应当完全不变。
     */
    @Test
    @DisplayName("budget 与 floor 一一对应时，去掉 budget 指纹数不变（冗余被数据坐实）")
    void budgetIsRedundant_whenItFollowsTheFloorFormula() {
        List<IntentRecord> records = List.of(
                record(request("r1", 1, 30, 87), "Llm"),
                record(request("r1", 2, 55, 87), "Llm"),
                record(request("r2", 1, 30, 50), "Llm"),
                record(request("r2", 2, 55, 50), "Llm"));

        List<CacheSimulator.FieldContribution> contributions =
                simulator.analyzeFieldContribution(records, FingerprintScheme.CURRENT);

        CacheSimulator.FieldContribution budget = contributions.stream()
                .filter(c -> c.field().equals("BUDGET")).findFirst().orElseThrow();

        assertThat(budget.marginallyRedundant())
                .as("budget 由 floor 唯一决定时，它对指纹的区分度贡献应当为零")
                .isTrue();
        assertThat(budget.splitFactor()).isEqualTo(1.0);

        // ⭐ 而真正坐实「可以去掉」的是这个：FLOOR 与 BUDGET 一一对应
        List<CacheSimulator.EquivalentPair> pairs =
                simulator.findEquivalentPairs(records, FingerprintScheme.CURRENT);
        assertThat(pairs)
                .as("floor 与 budget 应当被识别为一一对应——知道一个就知道另一个")
                .anySatisfy(p -> {
                    assertThat(Set.of(p.fieldA(), p.fieldB())).containsExactlyInAnyOrder("FLOOR", "BUDGET");
                    assertThat(p.trivial()).isFalse();
                    assertThat(p.distinctValues()).isEqualTo(2);
                });
    }

    /**
     * ⚠️ 单字段的「边际贡献为零」在字段高度相关时会大面积出现，那不是发现。
     *
     * <p>这条是实测暴露出来的：一批 6 局 × 2 层的真实流量跑出来，
     * <b>八个字段里七个的 marginallyRedundant 都是 true</b>，包括 PROFILE ——
     * 因为去掉任意一个，剩下的组合仍然能把样本分开。
     *
     * <p>本用例把这个局限<b>钉成一条测试</b>，免得日后有人看到一片 true
     * 就以为「这些字段都能删」。
     */
    @Test
    @DisplayName("字段高度相关时，多个字段的边际贡献同时为零——这是指标的局限，不是发现")
    void marginalRedundancy_isMisleadingWhenFieldsCorrelate() {
        // floor 与 profile 完全同步变化：每一局换一层，同时换一个画像桶
        List<IntentRecord> records = List.of(
                record(request("r1", 1, 30, 10), "Llm"),
                record(request("r2", 2, 55, 90), "Llm"));

        List<CacheSimulator.FieldContribution> contributions =
                simulator.analyzeFieldContribution(records, FingerprintScheme.CURRENT);

        long marginallyRedundant = contributions.stream()
                .filter(CacheSimulator.FieldContribution::marginallyRedundant).count();

        assertThat(marginallyRedundant)
                .as("多个字段同步变化时，它们的边际贡献会同时为零——所以这个指标不能单独用来做删除决策")
                .isGreaterThan(1);
    }

    /** 样本里某字段只有一种取值时，任何配对都"一一对应"，那是样本太单一。 */
    @Test
    @DisplayName("取值只有一种的字段对被标记为 trivial，不让空洞结论看起来像洞察")
    void singleValuedFields_areMarkedTrivial() {
        List<IntentRecord> records = List.of(record(request("r1", 1, 30, 87), "Llm"));

        List<CacheSimulator.EquivalentPair> pairs =
                simulator.findEquivalentPairs(records, FingerprintScheme.CURRENT);

        assertThat(pairs).isNotEmpty();
        assertThat(pairs).allSatisfy(p -> assertThat(p.trivial()).isTrue());
    }

    /**
     * ⚠️ 反面用例：<b>如果客户端传来的 budget 不遵守那个公式，它就不是冗余的。</b>
     *
     * <p>这条存在的意义是守住 D-24 里那句严谨表述 ——
     * 「在当前客户端实现下冗余」<b>不等于</b>「永远冗余」。
     * 服务端收到的是客户端传来的值，不该把一个假设写死成事实。
     */
    @Test
    @DisplayName("budget 不跟随 floor 时就不是冗余的——假设只在它成立时成立")
    void budgetIsNotRedundant_whenItVariesIndependently() {
        List<IntentRecord> records = List.of(
                record(request("r1", 1, 30, 87), "Llm"),
                record(request("r1", 1, 45, 87), "Llm"),   // 同一层，不同预算
                record(request("r2", 1, 60, 87), "Llm"));

        List<CacheSimulator.FieldContribution> contributions =
                simulator.analyzeFieldContribution(records, FingerprintScheme.CURRENT);

        CacheSimulator.FieldContribution budget = contributions.stream()
                .filter(c -> c.field().equals("BUDGET")).findFirst().orElseThrow();

        assertThat(budget.marginallyRedundant()).isFalse();
        assertThat(budget.splitFactor()).isGreaterThan(1.0);
    }

    @Test
    @DisplayName("去掉 floor 会把同一局的两层合并，指纹数下降、命中率上升")
    void droppingFloor_mergesFingerprintsAndRaisesHitRate() {
        // 两层的画像与候选集完全相同，只有 floor/budget 不同
        List<IntentRecord> records = new ArrayList<>();
        Instant t = Instant.parse("2026-08-14T00:00:00Z");
        for (int i = 0; i < 8; i++) {
            records.add(record(request("run-" + i, 1, 30, 87), "Llm", t.plusSeconds(i * 2L)));
            records.add(record(request("run-" + i, 2, 30, 87), "Llm", t.plusSeconds(i * 2L + 1)));
        }

        FingerprintScheme noFloor = FingerprintScheme.CURRENT
                .without(FingerprintScheme.Field.FLOOR)
                .without(FingerprintScheme.Field.BUDGET);

        List<CacheSimulator.SchemeResult> results =
                simulator.simulate(records, List.of(FingerprintScheme.CURRENT, noFloor));

        assertThat(results.get(1).distinctFingerprints())
                .as("去掉 floor 应把两层合并成一条指纹")
                .isLessThan(results.get(0).distinctFingerprints());
        assertThat(results.get(1).hitRate())
                .as("指纹合并之后候选攒得更快，命中率应当上升")
                .isGreaterThan(results.get(0).hitRate());
    }

    /**
     * ⭐ 样本量太小时必须自己说出来。
     *
     * <p>拿十几条数据说「我校准了参数」，正是本项目在演示数据那次犯过的错
     * （构造出产品跑不出来的状态，用户审阅时点破了它）。
     */
    @Test
    @DisplayName("样本量不足时报告自带警告，不让人拿十几条数据下结论")
    void smallSample_carriesWarning() {
        List<IntentRecord> few = List.of(record(request("r1", 1, 30, 87), "Llm"));

        CacheSimulator.SchemeResult r =
                simulator.simulate(few, List.of(FingerprintScheme.CURRENT)).get(0);

        assertThat(r.sampleWarning())
                .isNotNull()
                .contains("样本量")
                .contains("不能作为改动依据");
    }

    @Test
    @DisplayName("空样本不报错，返回全零结果")
    void emptySample_doesNotExplode() {
        CacheSimulator.SchemeResult r =
                simulator.simulate(List.of(), List.of(FingerprintScheme.CURRENT)).get(0);

        assertThat(r.totalRequests()).isZero();
        assertThat(r.hitRate()).isZero();
        assertThat(r.trace()).isEmpty();
    }

    // ── 测试数据 ──

    private static IntentRecord record(IntentRequest req, String source) {
        return record(req, source, Instant.parse("2026-08-14T00:00:00Z"));
    }

    private static IntentRecord record(IntentRequest req, String source, Instant at) {
        return IntentRecord.of(req,
                FingerprintScheme.CURRENT.compute(FingerprintInput.from(req)),
                CacheOutcome.MISS_EMPTY, 0, source, 200, 100L, at);
    }

    private static IntentRequest request(String runId, int floor, int budget, double build) {
        return new IntentRequest(
                1, runId, floor, 3, budget,
                Map.of(
                        "buildConcentration", build,
                        "combatEfficiency", 40.0,
                        "strategySwitch", 0.0,
                        "survivalPressure", 0.0,
                        "confidence", 0.9,
                        "dominantArchetype", "Archetype.Ranger"),
                List.of(new IntentRequest.AvailableRule("Rule.Ammo", "light", 10, List.of())),
                List.of("Enemy.Grunt"),
                List.of());
    }

    private static DirectorIntent someIntent() {
        return new DirectorIntent("Stable", Map.of("Enemy.Grunt", 1.0),
                List.of(new DirectorIntent.RuleIntent("Rule.Ammo", "light")), "台词", null);
    }
}
