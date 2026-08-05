package com.shanhai.director.ratelimit;

import static org.assertj.core.api.Assertions.assertThat;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

/**
 * 限流的单测 —— 纯内存，不起 Spring 上下文。
 *
 * <p>限流保护的是**上游 LLM 配额**，不是这台服务器。所以判据不是
 * "服务端扛不扛得住"，而是"别让别人花我的钱"。
 */
class RateLimiterTest {

    @Test
    @DisplayName("单局超过配额后拒绝 —— 挡住客户端 bug 导致的循环重试")
    void perRunQuotaIsEnforced() {
        RateLimiter limiter = new RateLimiter();
        String run = "run-A";

        // 正常一局只发 2 次（F1/F2）。配额 10 是给人工调试留的余量。
        for (int i = 0; i < 10; i++) {
            assertThat(limiter.tryConsume(run, "10.0.0.1"))
                    .as("第 %d 次应放行", i + 1).isTrue();
        }
        assertThat(limiter.tryConsume(run, "10.0.0.1"))
                .as("第 11 次应被拒 —— 超出单局配额").isFalse();
    }

    @Test
    @DisplayName("换 runId 不能绕过限流 —— IP 那道还在")
    void switchingRunIdStillHitsIpQuota() {
        RateLimiter limiter = new RateLimiter();
        String ip = "10.0.0.2";

        // 每次换新 runId，单局桶永远是满的；但 IP 桶每分钟只有 30 个。
        // 只有 runId 一个维度的话，随机生成 runId 就能无限刷。
        int allowed = 0;
        for (int i = 0; i < 40; i++) {
            if (limiter.tryConsume("run-" + i, ip)) {
                allowed++;
            }
        }
        assertThat(allowed)
                .as("换 runId 绕不过 IP 维度的每分钟 30 次")
                .isEqualTo(30);
    }

    @Test
    @DisplayName("不同局互不影响 —— 一个人刷不该拖累另一个人")
    void differentRunsHaveIndependentBuckets() {
        RateLimiter limiter = new RateLimiter();

        for (int i = 0; i < 10; i++) {
            limiter.tryConsume("run-X", "10.0.0.3");
        }
        assertThat(limiter.tryConsume("run-X", "10.0.0.3")).isFalse();

        // 换一局、换一个 IP（避开 IP 桶），应当照常放行
        assertThat(limiter.tryConsume("run-Y", "10.0.0.4"))
                .as("另一局不该被上一局的超限连累").isTrue();
    }

    @Test
    @DisplayName("runId 缺失时仍按 IP 限流 —— 不能因为契约不全就给无限额度")
    void missingRunIdStillLimitedByIp() {
        RateLimiter limiter = new RateLimiter();
        int allowed = 0;
        for (int i = 0; i < 40; i++) {
            if (limiter.tryConsume(null, "10.0.0.5")) {
                allowed++;
            }
        }
        assertThat(allowed).isEqualTo(30);
    }

    @Test
    @DisplayName("桶数量超阈值会被清理 —— 否则长期运行必然 OOM")
    void bucketsAreEvictedWhenTooMany() {
        RateLimiter limiter = new RateLimiter();
        for (int i = 0; i < 10_050; i++) {
            limiter.tryConsume("run-" + i, null);
        }
        assertThat(limiter.trackedRuns()).isGreaterThan(10_000);

        limiter.evictIfLarge();
        assertThat(limiter.trackedRuns())
                .as("清空后重新计数 —— runId 每局都变，不清理 Map 会无限增长")
                .isZero();
    }
}
