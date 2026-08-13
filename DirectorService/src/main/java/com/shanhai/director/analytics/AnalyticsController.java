package com.shanhai.director.analytics;

import java.time.Duration;
import java.time.Instant;
import java.util.ArrayList;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.springframework.format.annotation.DateTimeFormat;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.FingerprintScheme;
import com.shanhai.director.persistence.IntentRecord;
import com.shanhai.director.persistence.IntentRecordRepository;

/**
 * 决策数据的聚合分析（M5，决策 D-24）。
 *
 * <h2>它服务于哪条目标</h2>
 *
 * D-23 立的两条存在理由，第二条是「决策数据要能跨局聚合」。
 * 本控制器就是那条的兑现 —— <b>而它的第一个（也是唯一一个）实际用途，
 * 是校准 M3 那两个拍脑袋的缓存参数。</b>
 *
 * <p>所以这里<b>不是一个通用查询平台</b>：三个端点各自回答一个具体问题，
 * 不做条件组合、不做分页、不做导出。为一个不存在的需求加接口，
 * 是把简单问题变复杂 —— 这与本项目一贯的判断一致。
 *
 * <h2>⚠️ 安全</h2>
 *
 * <b>刻意不加认证</b>，但绑在独立路径 {@code /v1/analytics/**} 下。
 * 理由：本服务只在本机跑、不部署公网（D-23 已记：一个 502 的 Demo 比没有 Demo 更糟）。
 * <b>为一个不存在的部署形态付认证的复杂度，是本项目一直在拒绝的事。</b>
 *
 * <p>⚠️ 但如果哪天真要暴露，<b>这里必须先加认证</b> ——
 * 它读的是全部历史决策数据。这句话写在这里，是为了让那个决定不至于被忘掉。
 *
 * @since M5
 */
@RestController
@RequestMapping("/v1/analytics")
public class AnalyticsController {

    /** 不传时间范围时默认看多久。30 天足够覆盖本项目的全部数据。 */
    private static final Duration DEFAULT_WINDOW = Duration.ofDays(30);

    private final IntentRecordRepository repository;
    private final CacheSimulator simulator;

    public AnalyticsController(IntentRecordRepository repository, CacheSimulator simulator) {
        this.repository = repository;
        this.simulator = simulator;
    }

    /**
     * 这段时间的整体情况。
     *
     * <p>⭐ 最该看的一个数是 {@code avgRequestsPerFingerprint}：
     * <b>它小于候选数（3）就说明「候选攒不满」是必然的，而不是运气问题。</b>
     */
    @GetMapping("/summary")
    public ResponseEntity<?> summary(
            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant from,
            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant to) {

        List<IntentRecord> records = load(from, to);
        if (records.isEmpty()) {
            return ResponseEntity.ok(emptyReport());
        }

        Map<CacheOutcome, Integer> outcomes = new LinkedHashMap<>();
        Map<String, Integer> sources = new LinkedHashMap<>();
        Map<String, Integer> fingerprintCounts = new HashMap<>();
        List<Integer> latencies = new ArrayList<>(records.size());

        for (IntentRecord r : records) {
            outcomes.merge(r.cacheOutcome(), 1, Integer::sum);
            sources.merge(r.source(), 1, Integer::sum);
            fingerprintCounts.merge(r.fingerprint(), 1, Integer::sum);
            latencies.add(r.latencyMs());
        }
        latencies.sort(Integer::compareTo);

        int total = records.size();
        int hits = outcomes.getOrDefault(CacheOutcome.HIT, 0);
        int distinct = fingerprintCounts.size();

        Map<String, Object> body = new LinkedHashMap<>();
        body.put("totalRequests", total);
        body.put("windowFrom", records.get(0).createdAt());
        body.put("windowTo", records.get(total - 1).createdAt());
        body.put("cacheHitRate", round(total == 0 ? 0 : (double) hits / total));
        body.put("outcomeBreakdown", outcomes);
        body.put("sourceBreakdown", sources);
        body.put("distinctFingerprints", distinct);
        // ⭐ 这个数小于 maxVariants 就说明候选攒不满是必然的
        body.put("avgRequestsPerFingerprint", round((double) total / distinct));
        body.put("latencyMs", Map.of(
                "p50", percentile(latencies, 0.50),
                "p95", percentile(latencies, 0.95),
                "p99", percentile(latencies, 0.99)));
        body.put("note", "只统计到达服务端的请求。客户端降级本地的决策不在其中——"
                + "本表能回答『缓存效果怎么样』，回答不了『整体降级率是多少』。");
        return ResponseEntity.ok(body);
    }

    /**
     * ⭐ 本次开工的核心：在同一批历史请求上重放不同的指纹方案。
     *
     * <p>默认对照集是刻意选的，直接对应 D-24 里记下的两个待验证假设：
     * <ul>
     *   <li>{@code drop-budget} —— 验证 challengeBudget 是不是冗余的</li>
     *   <li>{@code drop-floor} —— 验证 floorIndex 是不是把一局切成了两半</li>
     *   <li>{@code bucket-*} / {@code variants-*} —— 才是原计划要校准的那两个参数</li>
     * </ul>
     *
     * <p><b>把「验证假设」排在「校准参数」前面是刻意的</b>：
     * 如果指纹本身被切得太碎，调桶宽是在给一个错误的结构做微调。
     */
    @GetMapping("/cache/simulate")
    public ResponseEntity<?> simulate(
            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant from,
            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant to,
            @RequestParam(required = false) List<Integer> buckets,
            @RequestParam(required = false) List<Integer> variants) {

        List<IntentRecord> records = load(from, to);
        if (records.isEmpty()) {
            return ResponseEntity.ok(emptyReport());
        }

        List<FingerprintScheme> schemes = new ArrayList<>();
        schemes.add(FingerprintScheme.CURRENT);
        schemes.add(FingerprintScheme.CURRENT.without(FingerprintScheme.Field.BUDGET));
        schemes.add(FingerprintScheme.CURRENT.without(FingerprintScheme.Field.FLOOR));
        schemes.add(FingerprintScheme.CURRENT
                .without(FingerprintScheme.Field.BUDGET)
                .without(FingerprintScheme.Field.FLOOR));
        // ⭐ 这一条是被真实数据逼出来的。
        //    最初的对照集只到「floor + budget 一起去掉」，在模拟流量上命中率翻倍，看着像结论了。
        //    但真实对局数据显示它毫无效果 —— 因为 confidence 也跟随 floor
        //    （前两层的历史长度固定，置信度就固定：F1→0.5、F2→0.7）。
        //    split-contribution 把三者标成了互相一一对应，**要合并两层必须三个一起去掉**。
        //    教训：连对照集本身都是拍的，也要由数据来纠正。
        schemes.add(FingerprintScheme.CURRENT
                .without(FingerprintScheme.Field.BUDGET)
                .without(FingerprintScheme.Field.FLOOR)
                .without(FingerprintScheme.Field.CONFIDENCE));
        schemes.add(FingerprintScheme.CURRENT.without(FingerprintScheme.Field.HISTORY));
        // 只保留「玩家是谁 + 能选什么」，把所有跟随层号的量全去掉
        schemes.add(new FingerprintScheme("only-player-and-options",
                FingerprintScheme.DEFAULT_PROFILE_BUCKET, FingerprintScheme.DEFAULT_MAX_VARIANTS,
                EnumSet.of(FingerprintScheme.Field.PROFILE, FingerprintScheme.Field.ARCHETYPE,
                        FingerprintScheme.Field.RULES, FingerprintScheme.Field.AVAIL_ARCHETYPES)));

        for (int b : buckets == null ? List.of(34, 50, 100) : buckets) {
            schemes.add(FingerprintScheme.CURRENT.withBucket(b));
        }
        for (int v : variants == null ? List.of(1, 2, 5) : variants) {
            schemes.add(FingerprintScheme.CURRENT.withMaxVariants(v));
        }

        List<CacheSimulator.SchemeResult> results = simulator.simulate(records, schemes);

        // trace 只用于测试里的正确性比对，不该出现在报告里（几百条噪音）
        List<Map<String, Object>> rows = new ArrayList<>();
        for (CacheSimulator.SchemeResult r : results) {
            Map<String, Object> row = new LinkedHashMap<>();
            row.put("scheme", r.schemeName());
            row.put("profileBucket", r.profileBucket());
            row.put("maxVariants", r.maxVariants());
            row.put("distinctFingerprints", r.distinctFingerprints());
            row.put("hitRate", round(r.hitRate()));
            row.put("avgRequestsPerFingerprint", round(r.avgRequestsPerFingerprint()));
            row.put("missEmpty", r.missEmpty());
            row.put("missWarmup", r.missWarmup());
            rows.add(row);
        }

        CacheSimulator.SchemeResult current = results.get(0);
        Map<String, Object> body = new LinkedHashMap<>();
        body.put("sampleSize", records.size());
        String warning = current.sampleWarning();
        if (warning != null) {
            body.put("sampleWarning", warning);
        }
        body.put("results", rows);
        body.put("note", "hitRate 是在历史请求上按时间顺序重放得出的模拟值，不是真实观测值。"
                + "重放假设『原本走 Llm/Cache 的请求，LLM 当时可用且会成功』——"
                + "这会让结果偏乐观，因为真实世界里 LLM 有失败率。");
        return ResponseEntity.ok(body);
    }

    /**
     * 指纹里每个字段各自把样本切成了多碎。
     *
     * <p>⭐ {@code redundant=true} 的字段<b>被别的字段完全决定</b>，
     * 留在指纹里不带来任何区分度，只是白占。
     */
    @GetMapping("/fingerprint/split-contribution")
    public ResponseEntity<?> splitContribution(
            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant from,
            @RequestParam(required = false)
            @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) Instant to) {

        List<IntentRecord> records = load(from, to);
        if (records.isEmpty()) {
            return ResponseEntity.ok(emptyReport());
        }

        Map<String, Object> body = new LinkedHashMap<>();
        body.put("sampleSize", records.size());
        body.put("baseDistinctFingerprints",
                new java.util.HashSet<>(records.stream()
                        .map(r -> FingerprintScheme.CURRENT.compute(r.input())).toList()).size());
        body.put("fields", simulator.analyzeFieldContribution(records, FingerprintScheme.CURRENT));
        // ⭐ equivalentPairs 才是「谁可以去掉」的答案。
        //    单字段的 marginallyRedundant 在字段高度相关时会大面积为 true（实测 8 个里 7 个），
        //    那不是发现，是这个指标本身的局限。
        body.put("equivalentPairs", simulator.findEquivalentPairs(records, FingerprintScheme.CURRENT));
        body.put("note", "splitFactor 越大表示该字段把样本切得越碎。"
                + "⚠️ marginallyRedundant=true 只表示『单独去掉它指纹数不变』，"
                + "不等于它可以被删——字段高度相关时可能每个的边际贡献都是零，而合起来是必需的。"
                + "真正可以去掉的是 equivalentPairs 里一一对应的那些（知道一个就知道另一个）。");
        return ResponseEntity.ok(body);
    }

    // ── 内部 ──

    private List<IntentRecord> load(Instant from, Instant to) {
        Instant end = to == null ? Instant.now().plusSeconds(60) : to;
        Instant start = from == null ? end.minus(DEFAULT_WINDOW) : from;
        return repository.findBetweenOrderByTime(start, end);
    }

    /**
     * 空数据集不是错误，是「还没打过局」。
     *
     * <p>返回 200 + 一句人话，而不是 404 或者一个全是 0 的报告 ——
     * <b>全是 0 的报告会让人以为「命中率 0%」，而事实是「没有数据」。</b>
     * 这两者的应对完全不同。
     */
    private Map<String, Object> emptyReport() {
        return Map.of(
                "sampleSize", 0,
                "message", "这段时间没有任何决策流水。先打几局，或确认服务启动时落库已启用"
                        + "（启动日志里会有 [M5] 那条）。");
    }

    private static double round(double v) {
        return Math.round(v * 10000.0) / 10000.0;
    }

    private static int percentile(List<Integer> sorted, double p) {
        if (sorted.isEmpty()) {
            return 0;
        }
        int idx = (int) Math.ceil(p * sorted.size()) - 1;
        return sorted.get(Math.max(0, Math.min(sorted.size() - 1, idx)));
    }
}
