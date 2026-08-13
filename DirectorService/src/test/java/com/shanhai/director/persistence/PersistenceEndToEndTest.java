package com.shanhai.director.persistence;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.time.Instant;
import java.util.List;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.http.MediaType;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;
import org.testcontainers.containers.MySQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import com.shanhai.director.api.IntentController;
import com.shanhai.director.cache.CacheOutcome;

/**
 * M5-2 的验收：<b>一次真实的决策请求，穿过控制器，落进真 MySQL。</b>
 *
 * <h2>这条和 {@link IntentRecorderTest} 验的不是一回事</h2>
 *
 * 那边用替身验「容错行为对不对」；<b>这边验「接线通不通」</b> ——
 * 控制器有没有真的把记录交出去、指纹有没有对上、口径有没有按约定来。
 *
 * <p>⚠️ 这类「接线」缺陷单测抓不到：每个零件都对，装起来不通。
 * <b>本项目为此吃过亏</b>（踩坑 #28：连不上服务端排查了一轮，真相是服务没启动）。
 */
@Testcontainers(disabledWithoutDocker = true)
@SpringBootTest
@AutoConfigureMockMvc
@TestPropertySource(properties = {
        // 不配 key → 走 ServerLocal stub 路径。
        // 本测试要验的是落库接线，不该真去烧 LLM 的钱。
        "shm.llm.api-key="
})
class PersistenceEndToEndTest {

    @Container
    @SuppressWarnings("resource")
    static final MySQLContainer<?> MYSQL = new MySQLContainer<>("mysql:8.0")
            .withDatabaseName("shm_director");

    @DynamicPropertySource
    static void datasource(DynamicPropertyRegistry registry) {
        registry.add("spring.datasource.url", MYSQL::getJdbcUrl);
        registry.add("spring.datasource.username", MYSQL::getUsername);
        registry.add("spring.datasource.password", MYSQL::getPassword);
    }

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private IntentRecordRepository repository;

    @Autowired
    private IntentRecorder recorder;

    @Autowired
    private JdbcTemplate jdbc;

    @BeforeEach
    void cleanTable() {
        jdbc.execute("TRUNCATE TABLE intent_request");
    }

    @Test
    @DisplayName("一次决策请求会落一行，且指纹与画像字段都对得上")
    void oneRequest_producesOneRow() throws Exception {
        Instant before = Instant.now().minusSeconds(5);

        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(requestJson("run-e2e", 1, 30)))
                .andExpect(status().isOk());

        assertThat(recorder.awaitDrained(5000)).isTrue();

        List<IntentRecord> rows = repository.findBetweenOrderByTime(before, Instant.now().plusSeconds(5));
        assertThat(rows).hasSize(1);

        IntentRecord r = rows.get(0);
        assertThat(r.runId()).isEqualTo("run-e2e");
        assertThat(r.input().floorIndex()).isEqualTo(1);
        assertThat(r.input().challengeBudget()).isEqualTo(30);
        assertThat(r.input().buildConcentration()).isEqualTo(87.0);
        assertThat(r.input().dominantArchetype()).isEqualTo("Archetype.Ranger");
        // 第一次见这条指纹
        assertThat(r.cacheOutcome()).isEqualTo(CacheOutcome.MISS_EMPTY);
        assertThat(r.variantCount()).isZero();
        // 没配 key → stub 路径。**source 必须如实反映实际走了哪条路**
        assertThat(r.source()).isEqualTo("ServerLocal");
        assertThat(r.httpStatus()).isEqualTo(200);
    }

    /**
     * ⚠️ 口径：<b>只记「走到了缓存查询」的请求。</b>
     *
     * <p>版本不符的 400 根本没查缓存，记进去会让命中率的分母不对，
     * 而本表存在的唯一目的就是校准缓存方案。
     */
    @Test
    @DisplayName("版本不符的 400 不落库——它没查过缓存，记了会污染命中率的分母")
    void rejectedRequest_isNotRecorded() throws Exception {
        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content(requestJson("run-bad", 1, 30).replace("\"schemaVersion\": 1", "\"schemaVersion\": 99")))
                .andExpect(status().isBadRequest());

        Thread.sleep(500);
        assertThat(repository.count()).isZero();
    }

    /**
     * ⭐ 同一层重复请求，落库要如实反映缓存状态的演进。
     *
     * <p>这是模拟器能工作的前提：它按时间顺序重放，
     * <b>如果落库记的顺序或状态不对，重放出来的命中率就没有意义。</b>
     */
    @Test
    @DisplayName("同一指纹连发多次，每次的 cache_outcome 如实反映当时的状态")
    void repeatedRequests_recordCacheStateEvolution() throws Exception {
        Instant before = Instant.now().minusSeconds(5);

        for (int i = 0; i < 3; i++) {
            mockMvc.perform(post(IntentController.INTENT_PATH)
                            .contentType(MediaType.APPLICATION_JSON)
                            .content(requestJson("run-repeat", 1, 30)))
                    .andExpect(status().isOk());
        }

        assertThat(recorder.awaitDrained(5000)).isTrue();

        List<IntentRecord> rows = repository.findBetweenOrderByTime(before, Instant.now().plusSeconds(5));
        assertThat(rows).hasSize(3);

        // 三条的指纹必须完全相同——同一层、同一画像、同一候选集
        assertThat(rows).extracting(IntentRecord::fingerprint).containsOnly(rows.get(0).fingerprint());

        // stub 路径不进缓存（只有真实 LLM 结果才缓存），所以三次都是 MISS_EMPTY。
        // ⭐ 这不是缺陷，是「缓存里不能混进占位内容」那条设计的直接结果——
        //    而落库如实记下了它，正说明落库没有替业务粉饰。
        assertThat(rows).extracting(IntentRecord::cacheOutcome)
                .containsOnly(CacheOutcome.MISS_EMPTY);
    }

    @Test
    @DisplayName("落库不拖慢决策：连发 20 次，队列深度不累积")
    void persistenceDoesNotBlockDecisions() throws Exception {
        long startedAt = System.nanoTime();
        for (int i = 0; i < 20; i++) {
            mockMvc.perform(post(IntentController.INTENT_PATH)
                            .contentType(MediaType.APPLICATION_JSON)
                            // ⚠️ 每次换 runId：限流是 10 次/局，同一 runId 连发 20 次会被 429
                            .content(requestJson("run-burst-" + i, 1, 30)))
                    .andExpect(status().isOk());
        }
        long elapsedMs = (System.nanoTime() - startedAt) / 1_000_000L;

        // 20 次请求走 stub 路径（不调 LLM），全程应远快于落库本身。
        // 这条断言宽松是刻意的：它守的是「有没有变成同步写库」这个数量级的问题，
        // 不是某个具体耗时——精确断言会把测试绑死在机器性能上（本项目踩坑 #17 的教训）。
        assertThat(elapsedMs)
                .as("20 次决策耗时 %d ms —— 若落库变成同步的，这里会是秒级", elapsedMs)
                .isLessThan(5000);

        assertThat(recorder.awaitDrained(5000)).isTrue();
        assertThat(repository.count()).isEqualTo(20);
        assertThat(recorder.droppedCount()).isZero();
        assertThat(recorder.failedCount()).isZero();
    }

    private static String requestJson(String runId, int floor, int budget) {
        return """
                {
                  "schemaVersion": 1,
                  "runId": "%s",
                  "floorIndex": %d,
                  "totalFloors": 3,
                  "challengeBudget": %d,
                  "profile": {
                    "buildConcentration": 87,
                    "combatEfficiency": 40,
                    "strategySwitch": 0,
                    "survivalPressure": 0,
                    "resourceSurplus": 50,
                    "confidence": 0.9,
                    "dominantArchetype": "Archetype.Ranger"
                  },
                  "availableRules": [
                    {"tag": "Rule.Ammo", "level": "light", "cost": 10, "conflictsWith": []}
                  ],
                  "availableArchetypes": ["Enemy.Grunt"],
                  "decisionHistory": []
                }
                """.formatted(runId, floor, budget);
    }
}
