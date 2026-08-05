package com.shanhai.director.ratelimit;

import java.time.Duration;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import io.github.bucket4j.Bandwidth;
import io.github.bucket4j.Bucket;

/**
 * 限流（M2）。
 *
 * <p><b>限流保护的是上游 LLM 配额，不是这台服务器。</b>
 * 一局游戏正常只发 2 次决策请求（F1/F2，F0 是观察层不走 Provider）。
 * 出现远高于此的量只有两种可能：客户端有 bug 在循环重试，或者有人在刷。
 * 两种都该拦——不是为了保护服务端 CPU（那点量算不上负载），
 * 是为了**别让别人花我的 LLM 钱**。
 *
 * <p>两个维度各自独立限流：
 * <ul>
 *   <li><b>runId</b>：一局一桶。防单局刷——客户端 bug 导致的循环重试会命中它</li>
 *   <li><b>客户端 IP</b>：防批量刷——换 runId 绕过第一道时会命中它</li>
 * </ul>
 * 只有 IP 维度的话，同一台机器正常打几局就会被误伤；
 * 只有 runId 维度的话，随机生成 runId 就能无限刷。两个都要。
 *
 * <p><b>429 不是错误，是设计内的降级路径。</b>客户端收到 429 会按降级级 2
 * 转本地 Provider，游戏照常进行，玩家零感知——FSHMRemoteProvider 里
 * 专门为它记了一条独立日志，就是为了让"被限流了"与"后端挂了"事后可区分。
 *
 * <h2>为什么用内存 Map 而不是 Redis</h2>
 * 现在只有一个服务实例，内存桶就是对的。Redis 是 M3 引入的（做缓存），
 * 到那时如果真要多实例部署，再把桶换成分布式的。
 * <b>现在上分布式限流是为一个不存在的部署形态付复杂度。</b>
 */
@Component
public class RateLimiter {

    private static final Logger log = LoggerFactory.getLogger(RateLimiter.class);

    /**
     * 一局的配额。
     *
     * <p>正常一局 2 次（F1/F2）。给到 10 是留足余量：
     * 人工用 SHM.DumpDecisionAsync 反复调试同一局时不该被拦。
     * 超过 10 次就不是正常游玩了。
     */
    private static final int PER_RUN_CAPACITY = 10;

    /**
     * 单 IP 每分钟配额。
     *
     * <p>一局最多 10 次、一局至少几分钟，30/分钟足够同时打好几局，
     * 又能挡住脚本刷。
     */
    private static final int PER_IP_CAPACITY = 30;

    private final Map<String, Bucket> runBuckets = new ConcurrentHashMap<>();
    private final Map<String, Bucket> ipBuckets = new ConcurrentHashMap<>();

    /**
     * 尝试消费一个令牌。
     *
     * @return true 放行；false 超限，调用方应返回 429
     */
    public boolean tryConsume(String runId, String clientIp) {
        // runId 缺失时不按 run 限流，只按 IP —— 缺 runId 本身是契约问题，
        // 但不该因此把请求算作"无限额度"，IP 那道仍然拦得住。
        if (runId != null && !runId.isBlank()) {
            Bucket runBucket = runBuckets.computeIfAbsent(runId, k -> newRunBucket());
            if (!runBucket.tryConsume(1)) {
                log.info("限流：runId={} 超出单局配额 {} —— 返回 429，客户端将降级本地",
                        runId, PER_RUN_CAPACITY);
                return false;
            }
        }

        if (clientIp != null && !clientIp.isBlank()) {
            Bucket ipBucket = ipBuckets.computeIfAbsent(clientIp, k -> newIpBucket());
            if (!ipBucket.tryConsume(1)) {
                log.info("限流：ip={} 超出每分钟配额 {} —— 返回 429，客户端将降级本地",
                        clientIp, PER_IP_CAPACITY);
                return false;
            }
        }

        return true;
    }

    private static Bucket newRunBucket() {
        // 一局的桶不回填：一局就那么多次，用完即止。
        // 回填的话长时间挂机的一局能刷出无限额度。
        return Bucket.builder()
                .addLimit(Bandwidth.builder()
                        .capacity(PER_RUN_CAPACITY)
                        .refillGreedy(PER_RUN_CAPACITY, Duration.ofHours(1))
                        .build())
                .build();
    }

    private static Bucket newIpBucket() {
        return Bucket.builder()
                .addLimit(Bandwidth.builder()
                        .capacity(PER_IP_CAPACITY)
                        .refillGreedy(PER_IP_CAPACITY, Duration.ofMinutes(1))
                        .build())
                .build();
    }

    /**
     * 清理过期桶。
     *
     * <p>不清理的话 runId 每局都变，Map 会无限增长 —— 一个长期运行的服务
     * 会因此 OOM。这是内存限流方案必须自己处理的事，
     * 用 Redis 时由 TTL 代劳（M3 若换过去，这段就可以删）。
     *
     * <p>做法很粗：超过阈值就整个清空。精确的 LRU 在这个量级上不值得——
     * 清空的后果只是所有人重新获得配额，而配额本来就是防滥用的软限制。
     */
    public void evictIfLarge() {
        if (runBuckets.size() > 10_000) {
            log.info("runId 桶数量 {} 超阈值，清空重来", runBuckets.size());
            runBuckets.clear();
        }
        if (ipBuckets.size() > 10_000) {
            ipBuckets.clear();
        }
    }

    /** 测试与指标用。 */
    public int trackedRuns() {
        return runBuckets.size();
    }
}
