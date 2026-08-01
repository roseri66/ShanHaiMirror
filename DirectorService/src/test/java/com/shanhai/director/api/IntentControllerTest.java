package com.shanhai.director.api;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.header;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.test.web.servlet.MvcResult;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;

/**
 * stub 端点的契约测试。
 *
 * <p>这一组守的是<b>跨语言契约</b>——服务端单独看没问题、但和 UE 侧对不上的那些错。
 * 路径写错、响应包了信封、配比和不为 1，都会表现为"每层降级本地、玩家零感知"，
 * 被降级链悄悄吞掉，不看日志根本发现不了。
 */
/*
 * ⚠️ 这一组测的是 **stub 路径**（未配置 key 时的行为）。
 *
 * properties 里把 key 显式置空，是为了让本地 application-local.yml 里的真实
 * key 影响不到测试。不这么做的话测试会真的去打 LLM——2026-08-01 第一次跑
 * 就是这样，单条 6.7 秒，而且断言对象悄悄从"我的代码"变成了"模型这次说了什么"。
 *
 * LLM 路径的测试见 IntentControllerLlmTest，那里用 @MockBean 造确定性响应。
 */
@SpringBootTest(properties = "shm.llm.api-key=")
@AutoConfigureMockMvc
class IntentControllerTest {

    @Autowired
    private MockMvc mockMvc;

    private final ObjectMapper mapper = new ObjectMapper();

    private static final String VALID_REQUEST = """
            {
              "schemaVersion": 1,
              "runId": "test-run",
              "floorIndex": 1,
              "totalFloors": 3,
              "challengeBudget": 30,
              "profile": {
                "buildConcentration": 87.0, "combatEfficiency": 72.0,
                "resourceSurplus": 40.0, "strategySwitch": 15.0,
                "survivalPressure": 22.0, "confidence": 0.9,
                "dominantArchetype": "Archetype.Ranger"
              },
              "availableRules": [{"tag": "Rule.Cooldown", "level": "light", "cost": 10}],
              "availableArchetypes": ["Enemy.Grunt", "Enemy.Tank", "Enemy.Rush", "Enemy.Shooter"],
              "decisionHistory": []
            }
            """;

    @Test
    @DisplayName("路径必须与 UE 侧 FSHMRemoteProvider::IntentPath 一致")
    void pathMatchesClientContract() throws Exception {
        // UE 侧那边同样有一条测试钉着这个字符串。改一边不改另一边 = 404 = 静默降级。
        assertThat(IntentController.INTENT_PATH).isEqualTo("/v1/director/intent");

        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andExpect(status().isOk());
    }

    @Test
    @DisplayName("响应体就是 Intent 本体，不带信封")
    void responseHasNoEnvelope() throws Exception {
        MvcResult result = mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andExpect(status().isOk())
                .andExpect(jsonPath("$.challengeLevel").exists())
                .andExpect(jsonPath("$.enemyWeights").exists())
                .andExpect(jsonPath("$.ruleIntents").isArray())
                .andExpect(jsonPath("$.narration").exists())
                // 信封的典型形态——一个都不该有
                .andExpect(jsonPath("$.data").doesNotExist())
                .andExpect(jsonPath("$.code").doesNotExist())
                .andExpect(jsonPath("$.result").doesNotExist())
                .andReturn();

        JsonNode root = mapper.readTree(result.getResponse().getContentAsString());
        assertThat(root.fieldNames()).toIterable()
                .containsExactlyInAnyOrder(
                        "challengeLevel", "enemyWeights", "ruleIntents", "narration", "reason");
    }

    @Test
    @DisplayName("Intent 里不得出现任何数值字段（数值只在客户端护栏之后产生）")
    void ruleIntentsCarryNoNumbers() throws Exception {
        MvcResult result = mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andReturn();

        JsonNode rules = mapper.readTree(result.getResponse().getContentAsString()).get("ruleIntents");
        for (JsonNode rule : rules) {
            assertThat(rule.fieldNames()).toIterable().containsExactlyInAnyOrder("tag", "level");
            assertThat(rule.has("multiplier")).isFalse();
            assertThat(rule.has("cost")).isFalse();
        }
    }

    @Test
    @DisplayName("配比之和必须为 1，否则被客户端 Schema 护栏拦下")
    void enemyWeightsSumToOne() throws Exception {
        MvcResult result = mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andReturn();

        JsonNode weights = mapper.readTree(result.getResponse().getContentAsString()).get("enemyWeights");
        double sum = 0.0;
        for (JsonNode w : weights) {
            sum += w.asDouble();
        }
        assertThat(sum).isCloseTo(1.0, org.assertj.core.data.Offset.offset(0.001));
    }

    @Test
    @DisplayName("stub 的规则必须买得起 F1 的预算 30")
    void stubRuleFitsInBudget() throws Exception {
        // stub 选的是 Rule.Cooldown light = cost 10。F1 预算 30、F2 预算 55，
        // 两层都够。买不起的话会被 Budget 护栏拦下并降级，M0 的端到端验收就白做了。
        MvcResult result = mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andReturn();

        JsonNode rules = mapper.readTree(result.getResponse().getContentAsString()).get("ruleIntents");
        assertThat(rules).hasSize(1);
        assertThat(rules.get(0).get("level").asText()).isEqualTo("light");
    }

    @Test
    @DisplayName("来源头必须报 ServerLocal —— M1 之前谎报 Llm 就是往证据链掺假")
    void sourceHeaderIsHonest() throws Exception {
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(VALID_REQUEST))
                .andExpect(header().string("X-SHM-Source", "ServerLocal"))
                .andExpect(header().string("X-SHM-Cache", "miss"))
                .andExpect(header().exists("X-SHM-Elapsed-Ms"));
    }

    @Test
    @DisplayName("契约版本不符必须明确拒绝，不静默错读")
    void rejectsUnsupportedSchemaVersion() throws Exception {
        String v2 = VALID_REQUEST.replace("\"schemaVersion\": 1", "\"schemaVersion\": 2");
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(v2))
                .andExpect(status().isBadRequest());
    }

    @Test
    @DisplayName("客户端多发未知字段不该 400 —— 两端不可能同时上线")
    void toleratesUnknownFields() throws Exception {
        String withExtra = VALID_REQUEST.replace(
                "\"runId\": \"test-run\",",
                "\"runId\": \"test-run\", \"someFutureField\": 42,");
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(withExtra))
                .andExpect(status().isOk());
    }
}
