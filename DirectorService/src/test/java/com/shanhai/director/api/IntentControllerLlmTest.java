package com.shanhai.director.api;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.header;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.mock.mockito.MockBean;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;

import com.shanhai.director.llm.LlmClient;

import reactor.core.publisher.Mono;

/**
 * LLM 路径的编排逻辑测试。
 *
 * <p><b>用 @MockBean 而不是真调 LLM</b>：这里要验证的是"上游成功/失败时
 * Controller 怎么做"，不是"模型今天说了什么"。真调的话断言对象会从我的代码
 * 变成模型的输出，非确定性、要花钱、CI 上还得放 key。
 *
 * <p>最关键的一条是 {@code upstreamFailureReturns503}：上游失败时
 * <b>绝不能悄悄回落 stub</b>。回落会让客户端拿到一个"成功"的响应，
 * 把 stub 的固定配比当成真实决策记进决策日志——那是往证据链里掺假。
 * 返回 5xx 客户端才会降级本地，日志里才会如实留下降级记录。
 */
@SpringBootTest
@AutoConfigureMockMvc
class IntentControllerLlmTest {

    @Autowired
    private MockMvc mockMvc;

    @MockBean
    private LlmClient llmClient;

    private static final String VALID_REQUEST = """
            {
              "schemaVersion": 1, "runId": "test-run", "floorIndex": 1, "totalFloors": 3,
              "challengeBudget": 30,
              "profile": {"buildConcentration": 87.0, "combatEfficiency": 72.0,
                          "strategySwitch": 15.0, "survivalPressure": 22.0, "confidence": 0.9,
                          "dominantArchetype": "Archetype.Ranger",
                          "primaryBuildTags": ["Build.Ranged"]},
              "availableRules": [{"tag": "Rule.Ammo", "level": "medium", "cost": 20,
                                  "conflictsWith": ["Rule.RangedDamage"]}],
              "availableArchetypes": ["Enemy.Grunt", "Enemy.Tank"],
              "decisionHistory": []
            }
            """;

    private static DirectorIntent fakeIntent() {
        return new DirectorIntent(
                "Counter",
                Map.of("Enemy.Grunt", 0.6, "Enemy.Tank", 0.4),
                List.of(new DirectorIntent.RuleIntent("Rule.Ammo", "medium")),
                "你的弓用得很好。但这一层，别指望站在原地。",
                "高置信度反制远程站桩。");
    }

    @Test
    @DisplayName("上游成功：原样下发 Intent，来源头报 Llm")
    void upstreamSuccessReportsLlm() throws Exception {
        when(llmClient.isAvailable()).thenReturn(true);
        when(llmClient.requestIntent(any())).thenReturn(Mono.just(fakeIntent()));

        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andExpect(status().isOk())
                .andExpect(header().string("X-SHM-Source", "Llm"))
                .andExpect(jsonPath("$.challengeLevel").value("Counter"))
                .andExpect(jsonPath("$.narration").value("你的弓用得很好。但这一层，别指望站在原地。"));
    }

    @Test
    @DisplayName("上游失败必须返 503，绝不悄悄回落 stub")
    void upstreamFailureReturns503() throws Exception {
        when(llmClient.isAvailable()).thenReturn(true);
        when(llmClient.requestIntent(any())).thenReturn(Mono.empty());

        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andExpect(status().isServiceUnavailable())
                // 回落 stub 的话这里会是 200 + 一份看起来像真决策的 body，
                // 客户端会把它当成功记进日志。必须是空 body。
                .andExpect(jsonPath("$.challengeLevel").doesNotExist());
    }

    @Test
    @DisplayName("LLM 不可用时走 stub，来源头如实报 ServerLocal")
    void unavailableFallsBackToStub() throws Exception {
        when(llmClient.isAvailable()).thenReturn(false);

        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andExpect(status().isOk())
                .andExpect(header().string("X-SHM-Source", "ServerLocal"))
                // stub 的标志性配比：偏 Tank 0.40
                .andExpect(jsonPath("$.enemyWeights['Enemy.Tank']").value(0.40));
    }
}
