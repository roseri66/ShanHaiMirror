package com.shanhai.director.llm;

import java.time.Duration;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import io.github.resilience4j.circuitbreaker.CircuitBreaker;
import io.github.resilience4j.circuitbreaker.CircuitBreakerConfig;
import io.github.resilience4j.circuitbreaker.CircuitBreakerRegistry;

/**
 * 上游韧性配置（M2）。
 *
 * <h2>为什么不加重试</h2>
 *
 * Resilience4j 提供了 Retry，但这里<b>刻意不用</b>，理由是时间预算根本不允许：
 *
 * <pre>
 *   服务端上游超时  10s
 *   客户端 Remote   12s   ← 只比上游多 2 秒
 *   玩法层最大等待  12s
 * </pre>
 *
 * 一次重试意味着最坏 20s，而客户端 12s 就放弃了——重试的结果**没人接收**，
 * 只是白白多烧一次 LLM 调用的钱。要支持重试就得把整条时间预算重新排，
 * 而层间过场只有那么长，玩家不会为了一次重试多等 8 秒。
 *
 * <p>失败时的正确行为不是重试，是<b>让客户端降级本地</b>——本地 Provider
 * 单独就是完整可玩的游戏，降级的体验损失远小于让玩家干等。
 * 这也正是不变量②存在的意义。
 *
 * <p>写下这段是因为"加了熔断怎么没加重试"是个会被反复问到的问题，
 * 答案不是"忘了"，是"时间预算不允许，而且降级比重试更划算"。
 */
@Configuration
public class ResilienceConfig {

    /** 熔断器名字。LlmClient 与指标都引用它，别写字符串字面量。 */
    public static final String LLM_BREAKER = "llmUpstream";

    @Bean
    public CircuitBreakerRegistry circuitBreakerRegistry() {
        CircuitBreakerConfig config = CircuitBreakerConfig.custom()
                // 按次数而非时间开窗：决策调用是低频事件（一层一次），
                // 时间窗在低频下几乎永远样本不足，等于熔断器不工作。
                .slidingWindowType(CircuitBreakerConfig.SlidingWindowType.COUNT_BASED)
                .slidingWindowSize(10)
                // 至少 5 次才判定，避免"开服第一次调用失败就熔断"。
                .minimumNumberOfCalls(5)
                // 10 次里 5 次失败就断开。LLM 上游本来就会偶发失败，
                // 阈值太低会让正常抖动触发熔断，反而降低可用性。
                .failureRateThreshold(50.0f)
                // 断开后 30 秒进入半开。**这个值与玩家体验挂钩**：
                // 一局 3 层、层间几分钟，30 秒意味着下一层就有机会恢复，
                // 不至于"一次抖动毁掉整局的 LLM 体验"。
                .waitDurationInOpenState(Duration.ofSeconds(30))
                .automaticTransitionFromOpenToHalfOpenEnabled(true)
                // 半开时放 3 个探测请求
                .permittedNumberOfCallsInHalfOpenState(3)
                // 慢调用也算失败：8 秒还没回来，即便最终成功，
                // 客户端（12s）也快等不及了，继续放行只是拖垮体验。
                .slowCallRateThreshold(50.0f)
                .slowCallDurationThreshold(Duration.ofSeconds(8))
                .build();

        return CircuitBreakerRegistry.of(config);
    }

    @Bean
    public CircuitBreaker llmCircuitBreaker(CircuitBreakerRegistry registry) {
        return registry.circuitBreaker(LLM_BREAKER);
    }
}
