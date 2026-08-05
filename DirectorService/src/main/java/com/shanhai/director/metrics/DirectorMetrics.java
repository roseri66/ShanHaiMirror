package com.shanhai.director.metrics;

import java.util.concurrent.TimeUnit;

import org.springframework.stereotype.Component;

import com.shanhai.director.cache.IntentCache;
import com.shanhai.director.llm.LlmClient;

import io.micrometer.core.instrument.Counter;
import io.micrometer.core.instrument.MeterRegistry;
import io.micrometer.core.instrument.Timer;

/**
 * 决策网关的指标（M4）。
 *
 * <p>暴露在 {@code /actuator/prometheus}。<b>不搭 Grafana 面板</b>——
 * D-23 修正¹：本机没有 Docker。指标数据是真的、可 curl 可查，只是没有图。
 *
 * <h2>⚠️ 这里没有"护栏拦截数"，这是刻意的</h2>
 *
 * 护栏在**客户端**（D-23 的核心否决：信任边界不在服务端）。
 * 服务端根本不知道自己返回的 Intent 有没有被拦下——那个信息只存在于
 * 玩家机器上的决策日志里。
 *
 * <p>要拿到它必须靠日志回流事后聚合（M5），<b>那是离线统计，不是实时指标</b>。
 * 在这里造一个 {@code shm_guardrail_rejects_total} 出来，只能是常量或猜测——
 * 那就是把服务端算不出来的东西当成观测结果发布，
 * 与踩坑 #23（构造出产品跑不出来的状态）是同一类问题。
 *
 * <p>设计文档第七节也点过这件事：「面板上必须标清哪些是实时、哪些是回流聚合，
 * 混在一起就是在骗自己」。既然现在没有回流，就一个都不放。
 */
@Component
public class DirectorMetrics {

    private final MeterRegistry registry;
    private final Timer decisionTimer;

    public DirectorMetrics(MeterRegistry registry, LlmClient llmClient, IntentCache cache) {
        this.registry = registry;

        // 决策耗时分布。README 里"实测 3.8–5.0s"是三次手工测量，
        // 这里让它变成持续曲线——P50/P99 比"我测了三次"有说服力得多。
        this.decisionTimer = Timer.builder("shm.director.latency")
                .description("一次决策请求的端到端耗时（含缓存命中，故分布是双峰的）")
                .publishPercentiles(0.5, 0.95, 0.99)
                .register(registry);

        // 熔断器状态。Gauge 而非 Counter：它是**当前状态**不是累计量。
        // 0=CLOSED 1=OPEN 2=HALF_OPEN 3=其它
        registry.gauge("shm.circuit.state", llmClient, c -> switch (c.circuitState()) {
            case "CLOSED" -> 0.0;
            case "OPEN" -> 1.0;
            case "HALF_OPEN" -> 2.0;
            default -> 3.0;
        });

        // 缓存命中率。**这个指标直接决定 6.1 的分桶策略调得好不好**——
        // 桶宽 20 与候选数 3 都是拍的，将来要靠它回头校准。
        registry.gauge("shm.cache.hit.ratio", cache, IntentCache::hitRatio);
        registry.gauge("shm.cache.hits", cache, c -> (double) c.hitCount());
        registry.gauge("shm.cache.misses", cache, c -> (double) c.missCount());

        // 订阅上游失败。用回调而非让 LlmClient 直接依赖本类，
        // 是因为本类的构造函数已经依赖 LlmClient（读熔断器状态），
        // 反向注入会形成循环依赖。
        llmClient.setFailureListener(this::recordUpstreamFailure);
    }

    /**
     * 记一次决策请求。
     *
     * @param source 实际走了哪条路径：Llm / Cache / ServerLocal。
     *               <b>如实标注</b>——这个标签会被拿去算"多少比例真的调了 LLM"，
     *               标错等于让统计说谎
     */
    public void recordDecision(String source, long elapsedNanos) {
        Counter.builder("shm.director.requests")
                .description("决策请求数，按实际来源分道")
                .tag("source", source)
                .register(registry)
                .increment();
        decisionTimer.record(elapsedNanos, TimeUnit.NANOSECONDS);
    }

    /**
     * 记一次上游失败。
     *
     * <p>{@code reason} 分道很重要：timeout / http / parse / circuit_open
     * 是四种完全不同的问题，混成一个"失败数"就失去了排查价值。
     */
    public void recordUpstreamFailure(String reason) {
        Counter.builder("shm.upstream.failures")
                .description("上游 LLM 调用失败数，按原因分道")
                .tag("reason", reason)
                .register(registry)
                .increment();
    }

    /** 记一次限流拒绝。**这不是错误计数**——429 是设计内的降级路径。 */
    public void recordRateLimited(String dimension) {
        Counter.builder("shm.ratelimit.rejected")
                .description("被限流拒绝的请求数（设计内的降级路径，非错误）")
                .tag("dimension", dimension)
                .register(registry)
                .increment();
    }
}
