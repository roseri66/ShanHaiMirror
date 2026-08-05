package com.shanhai.director.api;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;

import com.shanhai.director.llm.LlmClient;

/**
 * 限流与故障注入的端点级测试（M2）。
 *
 * <p>限流是有状态的，所以每个用例用不同的 runId，避免互相污染。
 */
@SpringBootTest(properties = "shm.llm.api-key=")
@AutoConfigureMockMvc
class IntentControllerResilienceTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private LlmClient llmClient;

    private static String requestWithRun(String runId) {
        return """
                {
                  "schemaVersion": 1, "runId": "%s", "floorIndex": 1, "totalFloors": 3,
                  "challengeBudget": 30,
                  "profile": {"confidence": 0.9},
                  "availableRules": [], "availableArchetypes": ["Enemy.Grunt"],
                  "decisionHistory": []
                }
                """.formatted(runId);
    }

    @Test
    @DisplayName("超出单局配额返回 429")
    void exceedingQuotaReturns429() throws Exception {
        when(llmClient.isAvailable()).thenReturn(false);   // 走 stub，不碰 LLM
        String run = "quota-test-" + System.nanoTime();

        for (int i = 0; i < 10; i++) {
            mockMvc.perform(post(IntentController.INTENT_PATH)
                            .contentType(MediaType.APPLICATION_JSON)
                            .content(requestWithRun(run)))
                    .andExpect(status().isOk());
        }

        // 第 11 次：429。**这不是错误，是设计内的降级路径** ——
        // 客户端收到 429 会按降级级 2 转本地 Provider，玩家零感知。
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(requestWithRun(run)))
                .andExpect(status().isTooManyRequests());
    }

    @Test
    @DisplayName("限流发生在调 LLM 之前 —— 被限流的请求不该烧配额")
    void rateLimitHappensBeforeLlmCall() throws Exception {
        // llmClient 可用，但超限的那次不该走到它
        when(llmClient.isAvailable()).thenReturn(true);
        when(llmClient.requestIntent(any())).thenThrow(
                new AssertionError("被限流的请求不该调用 LLM —— 那就白花钱了"));

        String run = "prelimit-" + System.nanoTime();
        // 先把配额耗尽（这些会撞上面那个 thenThrow，所以先让它不可用）
        when(llmClient.isAvailable()).thenReturn(false);
        for (int i = 0; i < 10; i++) {
            mockMvc.perform(post(IntentController.INTENT_PATH)
                    .contentType(MediaType.APPLICATION_JSON)
                    .content(requestWithRun(run)));
        }

        // 现在打开 LLM。第 11 次应该在限流处就被挡下，不进入 LLM 分支
        when(llmClient.isAvailable()).thenReturn(true);
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(requestWithRun(run)))
                .andExpect(status().isTooManyRequests());
    }
}

/**
 * 故障注入开关的测试（降级回归③的服务端侧）。
 *
 * <p>单独一个类是因为 {@code shm.mock-garbage} 要在上下文启动时生效，
 * 不能在同一个上下文里开关。
 */
@SpringBootTest(properties = "shm.llm.api-key=")
@AutoConfigureMockMvc
@TestPropertySource(properties = "shm.mock-garbage=true")
class IntentControllerMockGarbageTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private LlmClient llmClient;

    @Test
    @DisplayName("mock-garbage 开启时返回 200 + 无法解析的 body")
    void returnsGarbageWith200() throws Exception {
        when(llmClient.isAvailable()).thenReturn(false);

        // 这一条验的是服务端**能造出**这个故障；
        // 客户端拿到 200 之后会不会降级，由 UE 侧 FSHMJsonIntent 的
        // Malformed_FailsSafely 覆盖，以及人工把游戏跑起来验证。
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("""
                                {"schemaVersion":1,"runId":"garbage-test","floorIndex":1,
                                 "totalFloors":3,"challengeBudget":30,"profile":{},
                                 "availableRules":[],"availableArchetypes":[],"decisionHistory":[]}
                                """))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.garbage").value(1))
                // 关键：**没有任何 Intent 字段**，客户端解析器必然拒绝
                .andExpect(jsonPath("$.challengeLevel").doesNotExist())
                .andExpect(jsonPath("$.enemyWeights").doesNotExist());
    }
}
