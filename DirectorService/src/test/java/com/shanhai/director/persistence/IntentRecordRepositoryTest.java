package com.shanhai.director.persistence;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.test.context.DynamicPropertyRegistry;
import org.springframework.test.context.DynamicPropertySource;
import org.testcontainers.containers.MySQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

import com.shanhai.director.api.IntentRequest;
import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.FingerprintScheme;
import com.shanhai.director.cache.FingerprintInput;

/**
 * 落库的集成测试（M5，决策 D-24）。
 *
 * <h2>为什么起真 MySQL 而不用 H2</h2>
 *
 * 这张表的价值全在「能被查询」上：索引走不走得上、JSON 列收不收得下、
 * {@code DATETIME(3)} 的毫秒精度会不会被截断 —— <b>这些用 H2 测出来的只是
 * 「我以为 MySQL 是这样」</b>。这与本项目「别用自己编的流量验证效果类功能」是同一条教训。
 *
 * <p>⚠️ <b>{@code disabledWithoutDocker = true} 意味着没有 Docker 时这些用例会被跳过，
 * 而构建仍然是绿的。</b>报告测试结果时必须同时报「通过数」和「跳过数」——
 * <b>跳过不等于通过。</b>（另一个项目为此吃过亏，记为坑 P-22。）
 */
@Testcontainers(disabledWithoutDocker = true)
@SpringBootTest
class IntentRecordRepositoryTest {

    @Container
    @SuppressWarnings("resource") // 容器由 Testcontainers 的 JVM 关闭钩子回收
    static final MySQLContainer<?> MYSQL = new MySQLContainer<>("mysql:8.0")
            .withDatabaseName("shm_director");

    @DynamicPropertySource
    static void datasource(DynamicPropertyRegistry registry) {
        registry.add("spring.datasource.url", MYSQL::getJdbcUrl);
        registry.add("spring.datasource.username", MYSQL::getUsername);
        registry.add("spring.datasource.password", MYSQL::getPassword);
        // ⚠️ 刻意不设 spring.sql.init.mode ——
        // 建表由 DatabaseBootstrap 在 ApplicationReadyEvent 里做（理由见该类注释）。
        // 让测试走和生产完全相同的那条路径：**测试里额外开一个生产不开的开关，
        // 测的就不是生产会跑的东西了。**
    }

    @Autowired
    private IntentRecordRepository repository;

    @Autowired
    private JdbcTemplate jdbc;

    @BeforeEach
    void cleanTable() {
        // 每个用例拿干净的表。
        // 不用 @Transactional 回滚：本类要验的是「真的写进去了」，
        // 而事务回滚会让「写进去」和「没写进去」在断言之后看起来一样。
        jdbc.execute("TRUNCATE TABLE intent_request");
    }

    @Test
    @DisplayName("插入之后能原样读回来，包括参与指纹的每一个原始字段")
    void insertAndReadBack() {
        Instant now = Instant.now().truncatedTo(ChronoUnit.MILLIS);
        IntentRecord written = sampleRecord(now);

        repository.insertBatch(List.of(written));

        List<IntentRecord> read = repository.findBetweenOrderByTime(
                now.minusSeconds(60), now.plusSeconds(60));

        assertThat(read).hasSize(1);
        IntentRecord r = read.get(0);

        assertThat(r.runId()).isEqualTo("run-1");
        assertThat(r.schemaVersion()).isEqualTo(1);
        assertThat(r.fingerprint()).isEqualTo(written.fingerprint());
        assertThat(r.cacheOutcome()).isEqualTo(CacheOutcome.MISS_EMPTY);
        assertThat(r.variantCount()).isZero();
        assertThat(r.source()).isEqualTo("Llm");
        assertThat(r.httpStatus()).isEqualTo(200);
        assertThat(r.latencyMs()).isEqualTo(3765);

        // ⭐ 最要紧的一条：参与指纹计算的原始字段必须一个不差地回来。
        //    少任何一个，「换一套指纹方案会怎样」这个问题就永远回答不了了。
        assertThat(r.input()).isEqualTo(written.input());
    }

    /**
     * ⭐ 存了原始字段，就能用**另一套方案**重算指纹 —— 这正是本表存在的全部理由。
     *
     * <p>如果当初只存了指纹字符串，这个测试根本写不出来。
     */
    @Test
    @DisplayName("读回来的记录能直接喂给另一套指纹方案重算")
    void readBackRecord_canBeRefingerprinted() {
        Instant now = Instant.now().truncatedTo(ChronoUnit.MILLIS);
        repository.insertBatch(List.of(sampleRecord(now)));

        IntentRecord r = repository.findBetweenOrderByTime(
                now.minusSeconds(60), now.plusSeconds(60)).get(0);

        // 用当时那套方案重算，必须和当时存下来的一模一样 ——
        // 这是模拟器唯一的正确性锚点。
        assertThat(FingerprintScheme.CURRENT.compute(r.input())).isEqualTo(r.fingerprint());

        // 换一套方案，指纹就该不同（否则说明裁剪根本没生效）
        FingerprintScheme noBudget = FingerprintScheme.CURRENT
                .without(FingerprintScheme.Field.BUDGET);
        assertThat(noBudget.compute(r.input())).isNotEqualTo(r.fingerprint());
    }

    @Test
    @DisplayName("批量插入多条，按时间升序读回")
    void batchInsert_readInTimeOrder() {
        Instant base = Instant.now().truncatedTo(ChronoUnit.MILLIS).minusSeconds(10);

        repository.insertBatch(List.of(
                sampleRecord(base.plusMillis(300)),
                sampleRecord(base.plusMillis(100)),
                sampleRecord(base.plusMillis(200))));

        List<IntentRecord> read = repository.findBetweenOrderByTime(
                base, base.plusSeconds(60));

        assertThat(read).hasSize(3);
        assertThat(read.get(0).createdAt()).isBefore(read.get(1).createdAt());
        assertThat(read.get(1).createdAt()).isBefore(read.get(2).createdAt());
        assertThat(repository.count()).isEqualTo(3);
    }

    /**
     * JSON 列要收得下、也要在读回来时保持原样。
     *
     * <p>顺带验证转义：一个含引号的画像字段不该让整条记录插入失败。
     */
    @Test
    @DisplayName("profile_extra_json 存得进也读得出，含特殊字符也不炸")
    void jsonColumn_roundTrips() {
        Instant now = Instant.now().truncatedTo(ChronoUnit.MILLIS);
        String json = IntentRecord.extraProfileJson(Map.of(
                "resourceSurplus", 50.0,
                "weirdField", "含\"引号\"与\\反斜杠"));

        IntentRecord rec = new IntentRecord("run-json", 1, sampleInput(),
                "fp-json", CacheOutcome.HIT, 3, "Cache", 200, 2, json, now);

        repository.insertBatch(List.of(rec));

        IntentRecord read = repository.findBetweenOrderByTime(
                now.minusSeconds(60), now.plusSeconds(60)).get(0);
        assertThat(read.profileExtraJson()).contains("resourceSurplus");
        assertThat(read.profileExtraJson()).contains("引号");
    }

    /**
     * ⭐ 索引真的被用上了 —— 光建索引不算完，要确认优化器选它。
     *
     * <p>联合索引 {@code (fingerprint, created_at)} 的顺序是刻意的：
     * <b>等值列在前、范围列在后</b>。反过来放的话范围查询之后的列会失效，
     * fingerprint 就用不上了。
     */
    @Test
    @DisplayName("按指纹+时间查询走上了联合索引，不是全表扫")
    void fingerprintQuery_usesIndex() {
        Instant now = Instant.now().truncatedTo(ChronoUnit.MILLIS);
        repository.insertBatch(List.of(sampleRecord(now)));

        Map<String, Object> plan = jdbc.queryForMap(
                "EXPLAIN SELECT id FROM intent_request"
                        + " WHERE fingerprint = ? AND created_at >= ?",
                "fp-x", java.sql.Timestamp.from(now.minusSeconds(60)));

        assertThat(String.valueOf(plan.get("key"))).isEqualTo("idx_fingerprint_created");
        assertThat(String.valueOf(plan.get("type"))).isNotEqualTo("ALL");
    }

    // ── 测试数据 ──

    private static IntentRecord sampleRecord(Instant at) {
        IntentRequest req = sampleRequest();
        return IntentRecord.of(req,
                FingerprintScheme.CURRENT.compute(FingerprintInput.from(req)),
                CacheOutcome.MISS_EMPTY, 0, "Llm", 200, 3765L, at);
    }

    private static FingerprintInput sampleInput() {
        return FingerprintInput.from(sampleRequest());
    }

    private static IntentRequest sampleRequest() {
        return new IntentRequest(
                1, "run-1", 1, 3, 30,
                Map.of(
                        "buildConcentration", 87.0,
                        "combatEfficiency", 40.0,
                        "strategySwitch", 0.0,
                        "survivalPressure", 0.0,
                        "resourceSurplus", 50.0,
                        "confidence", 0.9,
                        "dominantArchetype", "Archetype.Ranger"),
                List.of(new IntentRequest.AvailableRule("Rule.Ammo", "light", 10, List.of())),
                List.of("Enemy.Grunt"),
                List.of());
    }
}
