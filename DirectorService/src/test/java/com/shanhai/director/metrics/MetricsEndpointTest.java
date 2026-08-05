package com.shanhai.director.metrics;

import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.actuate.observability.AutoConfigureObservability;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.MvcResult;

import com.shanhai.director.api.IntentController;
import com.shanhai.director.llm.LlmClient;

import org.assertj.core.api.Assertions;

/**
 * 指标端点测试（M4）。
 *
 * <p>除了"指标出得来"，这里还守两条安全线：
 * <b>不该暴露的 actuator 端点必须关着</b>，且<b>指标里不能出现 API key</b>。
 */
/*
 * @AutoConfigureObservability 是必须的：**Spring Boot 在测试里默认禁用指标导出**，
 * 不加的话 /actuator/prometheus 返回 404，而生产环境是正常的。
 *
 * 这个坑差点让两条测试变成假绿——它们只断言 doesNotContain，
 * 而 404 的响应体天然不包含任何东西，端点不存在时照样通过。
 * 所以下面每条都先断言端点真的活着（isOk + 非空），再验内容。
 * **只写否定断言的测试没有牙**，这是本项目第二次撞上同一个模式。
 */
@SpringBootTest(properties = "shm.llm.api-key=")
@AutoConfigureMockMvc
@AutoConfigureObservability
class MetricsEndpointTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private LlmClient llmClient;

    private static final String REQ = """
            {"schemaVersion":1,"runId":"metrics-%d","floorIndex":1,"totalFloors":3,
             "challengeBudget":30,"profile":{"confidence":0.9},
             "availableRules":[],"availableArchetypes":["Enemy.Grunt"],"decisionHistory":[]}
            """;

    @Test
    @DisplayName("六项指标都在 /actuator/prometheus 里")
    void allMetricsArePresent() throws Exception {
        when(llmClient.isAvailable()).thenReturn(false);
        when(llmClient.circuitState()).thenReturn("CLOSED");

        // 先打一次请求，让 Counter 与 Timer 产生数据（无数据的 meter 不会出现）
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(REQ.formatted(System.nanoTime())))
                .andExpect(status().isOk());

        MvcResult result = mockMvc.perform(get("/actuator/prometheus"))
                .andExpect(status().isOk())
                .andReturn();
        String body = result.getResponse().getContentAsString();

        Assertions.assertThat(body)
                .contains("shm_director_requests_total")
                .contains("shm_director_latency_seconds")
                .contains("shm_circuit_state")
                .contains("shm_cache_hit_ratio");

        // source 标签必须如实分道，否则"多少比例真调了 LLM"就算不出来
        Assertions.assertThat(body).contains("source=\"ServerLocal\"");
    }

    @Test
    @DisplayName("⚠️ 不得出现护栏拦截指标 —— 服务端根本没有这个数据")
    void noFabricatedGuardrailMetric() throws Exception {
        when(llmClient.circuitState()).thenReturn("CLOSED");

        MvcResult result = mockMvc.perform(get("/actuator/prometheus"))
                .andExpect(status().isOk())   // ★ 先确认端点活着，否则下面的否定断言没有意义
                .andReturn();
        String body = result.getResponse().getContentAsString();
        Assertions.assertThat(body).as("指标输出不该是空的").isNotBlank();

        // 护栏在客户端（D-23 的核心否决）。服务端不知道自己返回的 Intent
        // 有没有被拦下——那个信息只在玩家机器的决策日志里。
        // 在这里造一个出来只能是常量或猜测，与踩坑 #23 同类。
        Assertions.assertThat(body)
                .as("服务端算不出护栏拦截数，就一个都不要放")
                .doesNotContain("guardrail")
                .doesNotContain("reject_rate");
    }

    @Test
    @DisplayName("敏感 actuator 端点必须关着")
    void sensitiveEndpointsAreClosed() throws Exception {
        // env / configprops 会把配置打出来。Spring 对含 key/secret 的项
        // 有脱敏启发式，但依赖它保护 API key 是不必要的风险。
        mockMvc.perform(get("/actuator/env")).andExpect(status().isNotFound());
        mockMvc.perform(get("/actuator/configprops")).andExpect(status().isNotFound());
        mockMvc.perform(get("/actuator/beans")).andExpect(status().isNotFound());
        mockMvc.perform(get("/actuator/heapdump")).andExpect(status().isNotFound());
    }

    @Test
    @DisplayName("指标输出里不得出现任何疑似 key 的内容")
    void metricsLeakNoSecrets() throws Exception {
        when(llmClient.circuitState()).thenReturn("CLOSED");

        MvcResult result = mockMvc.perform(get("/actuator/prometheus"))
                .andExpect(status().isOk())   // ★ 同上：端点不活着的话下面全是空断言
                .andReturn();
        String body = result.getResponse().getContentAsString().toLowerCase();
        Assertions.assertThat(body).as("指标输出不该是空的").isNotBlank();

        Assertions.assertThat(body)
                .doesNotContain("sk-")
                .doesNotContain("api-key")
                .doesNotContain("apikey")
                .doesNotContain("authorization");
    }

    @Test
    @DisplayName("health 端点可用但不吐内部细节")
    void healthIsAvailableButTerse() throws Exception {
        mockMvc.perform(get("/actuator/health"))
                .andExpect(status().isOk())
                .andExpect(content().string(org.hamcrest.Matchers.containsString("UP")))
                // show-details: never —— 不暴露各组件的内部状态
                .andExpect(content().string(org.hamcrest.Matchers.not(
                        org.hamcrest.Matchers.containsString("components"))));
    }
}
