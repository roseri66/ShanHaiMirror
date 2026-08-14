package com.shanhai.director.api;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.RequestHeader;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Optional;

import com.shanhai.director.cache.IntentCache;
import com.shanhai.director.persistence.IntentRecord;
import com.shanhai.director.persistence.IntentRecorder;
import com.shanhai.director.llm.LlmClient;
import com.shanhai.director.metrics.DirectorMetrics;
import com.shanhai.director.ratelimit.RateLimiter;

import jakarta.servlet.http.HttpServletRequest;

/**
 * 决策端点。
 *
 * <p><b>M0 阶段这是一个 stub</b>：返回固定 Intent，不调 LLM。
 * 存在的意义是把整条链路先打通——客户端能发出合规的上行请求、
 * 服务端能解析、响应能过客户端的四道护栏并落进报告卡。
 * 真实 LLM 调用是 M1 的事。
 *
 * <p><b>护栏不在这里</b>，也永远不会搬过来（D-23 的核心否决）。
 * 服务端返回的 Intent 与其它 Provider 的输出一样，要过客户端四道护栏，
 * 不因为"是自己的服务"就有免检特权。
 */
@RestController
public class IntentController {

    private static final Logger log = LoggerFactory.getLogger(IntentController.class);

    private final LlmClient llmClient;
    private final RateLimiter rateLimiter;
    private final IntentCache intentCache;
    private final DirectorMetrics metrics;

    /**
     * M5（决策 D-24）落库。
     *
     * <p>⚠️ <b>它绝不能影响主链路</b>：{@code record()} 是非阻塞的，队列满了直接丢。
     * 数据库挂了、写库报错，决策照常返回 —— 这是 D-24 的硬约束①。
     */
    private final IntentRecorder recorder;

    /**
     * 故障注入开关，用于 D-23 要求的第三条降级回归
     * 「后端返 200 但 body 是垃圾」。
     *
     * <p>用启动参数而非代码分支，是为了让这条回归<b>能对着真实进程跑</b>——
     * 单测里 mock 一个垃圾响应只能验证解析器，验证不了
     * "客户端拿到 200 之后会不会真的降级"。
     *
     * <p>启动方式：{@code mvn spring-boot:run -Dspring-boot.run.arguments=--shm.mock-garbage=true}
     */
    @Value("${shm.mock-garbage:false}")
    private boolean mockGarbage;

    public IntentController(LlmClient llmClient, RateLimiter rateLimiter,
                            IntentCache intentCache, DirectorMetrics metrics,
                            IntentRecorder recorder) {
        this.llmClient = llmClient;
        this.rateLimiter = rateLimiter;
        this.intentCache = intentCache;
        this.metrics = metrics;
        this.recorder = recorder;
    }

    /** 上行路径。<b>必须与 UE 侧 FSHMRemoteProvider::IntentPath 一致</b>，那边有测试钉着。 */
    public static final String INTENT_PATH = "/v1/director/intent";

    /** 本服务能处理的上行契约版本。客户端发来更高的版本就明确拒绝，不猜。 */
    private static final int SUPPORTED_SCHEMA_VERSION = 1;

    /**
     * ⭐ D-25：客户端在调试作弊生效时带上的头，值是倍率摘要（如 {@code "dmg=4.00,hp=0.40"}）。
     *
     * <p>它落进 {@code debug_flags} 列，用来把作弊采来的样本和真实游玩的样本**永久分开**。
     * 缺了它，两批数据混进同一张表之后任何一次重放都无法解释 ——
     * 那正是踩坑 #31（测试数据污染开发库）换了个场景。
     */
    static final String DEBUG_HEADER = "X-SHM-Debug";

    @PostMapping(INTENT_PATH)
    public ResponseEntity<?> intent(@RequestBody(required = false) IntentRequest request,
                                    @RequestHeader(value = DEBUG_HEADER, required = false) String debugFlags,
                                    HttpServletRequest http) {
        final long startedAt = System.nanoTime();

        // --- 版本不符：明确拒绝，不静默错读 ---
        // 字段同名而语义已变时，返回一个"看起来对"的错误结论
        // 比返回 400 糟糕得多。这正是当初把 schemaVersion 钉在顶层第一个
        // 字段换来的红利。客户端收到非 200 会照常降级本地，玩家零感知。
        if (request == null || request.schemaVersion() == null) {
            log.warn("请求缺少 schemaVersion，拒绝");
            return ResponseEntity.badRequest().build();
        }
        if (request.schemaVersion() != SUPPORTED_SCHEMA_VERSION) {
            log.warn("上行契约版本不符：收到 {}，本服务支持 {} —— 拒绝，客户端将降级本地",
                    request.schemaVersion(), SUPPORTED_SCHEMA_VERSION);
            return ResponseEntity.badRequest().build();
        }

        // --- 限流。429 不是错误，是设计内的降级路径 ---
        // 保护的是上游 LLM 配额，不是这台服务器。客户端收到 429 会降本地，
        // 玩家零感知，且客户端为它记了独立日志（"被限流"与"后端挂了"可区分）。
        RateLimiter.Decision limit = rateLimiter.tryConsumeDetailed(request.runId(), http.getRemoteAddr());
        if (!limit.allowed()) {
            metrics.recordRateLimited(limit.dimension());
            rateLimiter.evictIfLarge();
            return ResponseEntity.status(429).build();
        }

        // --- 故障注入：返 200 但 body 是垃圾（降级回归③）---
        // 放在限流之后、LLM 之前：这条回归验的是客户端拿到"成功但无用"的响应
        // 会不会降级，不该顺带消耗 LLM 配额。
        if (mockGarbage) {
            log.warn("!! mock-garbage 已开启，返回 200 + 垃圾 body（仅供降级回归测试）");
            final long ms = (System.nanoTime() - startedAt) / 1_000_000L;
            return ResponseEntity.ok()
                    .header("X-SHM-Source", "MockGarbage")
                    .header("X-SHM-Elapsed-Ms", String.valueOf(ms))
                    .body(java.util.Map.of("garbage", 1));
        }

        log.info("收到决策请求：runId={} floor={}/{} budget={} 候选规则={} 候选原型={} 历史={}层",
                request.runId(), request.floorIndex(), request.totalFloors(),
                request.challengeBudget(),
                request.availableRules() == null ? 0 : request.availableRules().size(),
                request.availableArchetypes() == null ? 0 : request.availableArchetypes().size(),
                request.decisionHistory() == null ? 0 : request.decisionHistory().size());

        // --- 缓存（M3）---
        // 命中的是同一"决策情境"而非同一请求：指纹对连续量分桶，
        // 87 分和 85 分的玩家算同一类。同一指纹存 3 条、随机取 1 条，
        // 是为了不让同类玩家听到同一句台词——白泽的人格是体验核心之一。
        // M5 起用 lookupDetailed：它比 lookup 多带回「为什么是这个结果」与指纹本身。
        // 落库要记的正是这两样——而指纹计算不便宜（要排序拼接三个集合），
        // 顺手带回来比事后重算一遍好，也少一处算法漂移的机会。
        IntentCache.LookupResult lookup = intentCache.lookupDetailed(request);
        if (lookup.intent().isPresent()) {
            return respond(request, lookup, lookup.intent().get(), "Cache", "hit", startedAt, debugFlags);
        }

        // --- 真实 LLM 路径 ---
        if (llmClient.isAvailable()) {
            DirectorIntent fromLlm = llmClient.requestIntent(request).block();
            if (fromLlm != null) {
                // 只缓存**真实 LLM 结果**。stub 与降级产物都不进缓存：
                // 缓存里混进占位内容，之后每次命中都在发假决策。
                intentCache.store(request, fromLlm);
                return respond(request, lookup, fromLlm, "Llm", "miss", startedAt, debugFlags);
            }
            // 上游失败：**返回 5xx 而不是悄悄回落 stub**。
            // 回落的话客户端会拿到一个"成功"的响应，把 stub 的固定配比
            // 当成真实决策记进日志——那是往证据链里掺假。
            // 返回 5xx 客户端就降级本地，玩家零感知，日志如实记降级。
            log.warn("上游 LLM 交不出结果，返回 503 让客户端降级本地");
            // ⭐ 这条**也要落库**。它虽然没产出决策，但它确实查了缓存——
            //    模拟器要重放缓存状态的演进，漏掉它会让重放的序列和真实的对不上。
            //    source 记成 Upstream503：模拟器据此知道「这次没有往缓存里放东西」。
            record(request, lookup, "Upstream503", 503, startedAt, debugFlags);
            return ResponseEntity.status(503).build();
        }

        // --- 未配置 key：stub 路径（M0 行为，保留供无 key 时演示）---
        return respond(request, lookup, buildStubIntent(), "ServerLocal", "miss", startedAt, debugFlags);
    }

    /**
     * 统一出口。
     *
     * <p>{@code source} 必须如实反映**这次实际走了哪条路径**：
     * Llm = 真调了上游；ServerLocal = 服务端自己产的固定 Intent。
     * 客户端会把它记进决策日志的 trace，**谎报来源等于往证据链里掺假**。
     * 这与客户端"UI 显示实际发生了什么、不是配置成了什么"（踩坑 #20）是同一条线。
     */
    private ResponseEntity<?> respond(IntentRequest request, IntentCache.LookupResult lookup,
                                      DirectorIntent intent, String source, String cache,
                                      long startedAt, String debugFlags) {
        final long elapsedNanos = System.nanoTime() - startedAt;
        final long elapsedMs = elapsedNanos / 1_000_000L;
        metrics.recordDecision(source, elapsedNanos);
        record(request, lookup, source, 200, startedAt, debugFlags);
        return ResponseEntity.ok()
                .header("X-SHM-Source", source)
                .header("X-SHM-Cache", cache)
                .header("X-SHM-Elapsed-Ms", String.valueOf(elapsedMs))
                .body(intent);
    }

    /**
     * 把这次请求交给落库（M5，决策 D-24）。
     *
     * <h2>⚠️ 口径：只记「走到了缓存查询」的请求</h2>
     *
     * 版本不符的 400 和被限流的 429 <b>不记</b> —— 它们根本没查缓存，
     * 记进去会让命中率的分母不对，而本表存在的唯一目的就是校准缓存方案。
     * <b>口径写在这里，也写在表注释里</b>：日后看数据的人一定会问这个。
     *
     * <p>{@code source} 顺带承担了一个模拟器需要的信息：
     * <b>只有 {@code "Llm"} 意味着「这次往缓存里放了东西」</b>
     * （缓存命中不放、stub 不放、503 没东西可放）。重放时靠它决定要不要模拟一次 store。
     */
    private void record(IntentRequest request, IntentCache.LookupResult lookup,
                        String source, int httpStatus, long startedAt, String debugFlags) {
        // ⚠️ 整段兜住：落库的任何问题都不能影响已经算好的决策。
        // 这里兜的是「构造记录时出意外」（比如画像里有个诡异的值），
        // recorder.record() 自己兜的是「队列满」与「数据库不可用」。
        try {
            recorder.record(IntentRecord.of(request, lookup.fingerprint(), lookup.outcome(),
                    lookup.variantCount(), source, httpStatus,
                    (System.nanoTime() - startedAt) / 1_000_000L, debugFlags, Instant.now()));
        } catch (RuntimeException e) {
            log.warn("[M5] 构造流水记录失败，本条不落库：{}", e.toString());
        }
    }

    /**
     * M0 的固定 Intent。
     *
     * <p>三个刻意的选择：
     * <ul>
     *   <li><b>配比明显偏 Tank</b>（0.40）——本地 Provider 不会产出这个分布，
     *       所以在游戏里一眼能认出"这一层的配比确实来自 stub"，
     *       这正是 M0 的端到端验收判据</li>
     *   <li><b>只选一条 light 规则</b>（cost 10）——F1 预算 30、F2 预算 55，
     *       两层都买得起，不会被 Budget 护栏拦下</li>
     *   <li><b>选 Rule.Cooldown</b>——它在 RuleTable.csv 里 ConflictsWith 为空，
     *       完全绕开 Conflict 护栏。stub 的目的是验证链路通不通，
     *       不是去触发护栏（那是 M2 的降级回归要做的事）</li>
     * </ul>
     *
     * <p>台词直说自己是 stub。**不伪装成 LLM 生成的内容**：
     * 万一它出现在截图或演示里，也能自证来历，不会让人误以为是模型说的话。
     */
    private DirectorIntent buildStubIntent() {
        // LinkedHashMap 保序，日志和抓包里看着稳定
        final Map<String, Double> weights = new LinkedHashMap<>();
        weights.put("Enemy.Tank", 0.40);
        weights.put("Enemy.Rush", 0.30);
        weights.put("Enemy.Grunt", 0.20);
        weights.put("Enemy.Shooter", 0.10);   // 和 = 1.00，Schema 护栏要求

        return new DirectorIntent(
                "Pressure",
                weights,
                List.of(new DirectorIntent.RuleIntent("Rule.Cooldown", "light")),
                "【DirectorService stub】这句话来自服务端占位实现，不是 LLM 生成的。",
                "M0 固定响应：验证链路连通，不代表任何真实决策逻辑。"
        );
    }
}
