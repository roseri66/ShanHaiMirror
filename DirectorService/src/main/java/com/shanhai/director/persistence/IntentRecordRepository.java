package com.shanhai.director.persistence;

import java.sql.Timestamp;
import java.time.Instant;
import java.util.List;

import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.jdbc.core.RowMapper;
import org.springframework.stereotype.Repository;

import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.FingerprintInput;

/**
 * {@code intent_request} 表的读写（M5，决策 D-24）。
 *
 * <h2>为什么是 JdbcTemplate 而不是 JPA / MyBatis</h2>
 *
 * 本服务对这张表只做两件事：<b>批量插入</b>和<b>按时间范围全量读出来跑模拟</b>。
 * 没有对象图、没有懒加载、没有关联映射 —— 引一整套 ORM 的抽象收益抵不过它的复杂度。
 * SQL 全手写、全可见，与本项目「优先简单稳定」一致。
 *
 * <h2>⚠️ 这里不做异常兜底</h2>
 *
 * 本类<b>如实抛出</b> {@code DataAccessException}。
 * 「落库失败不能影响主链路」这条硬约束由调用方（{@code IntentRecorder}）负责 ——
 * <b>把容错放在最底层，会让上层再也无法区分「写成功了」和「写失败但被吞了」</b>，
 * 而那正是 D-24 要求 {@code shm_persist_failed_total} 这条指标存在的意义。
 *
 * @since M5
 */
@Repository
public class IntentRecordRepository {

    private static final String INSERT_SQL = """
            INSERT INTO intent_request (
                run_id, floor_index, schema_version,
                challenge_budget, build_concentration, combat_efficiency,
                strategy_switch, survival_pressure, confidence, dominant_archetype,
                available_rules_key, available_arch_key, history_tags_key,
                fingerprint, cache_outcome, variant_count, source,
                http_status, latency_ms, profile_extra_json, created_at
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """;

    private static final String SELECT_COLUMNS = """
            run_id, floor_index, schema_version,
            challenge_budget, build_concentration, combat_efficiency,
            strategy_switch, survival_pressure, confidence, dominant_archetype,
            available_rules_key, available_arch_key, history_tags_key,
            fingerprint, cache_outcome, variant_count, source,
            http_status, latency_ms, profile_extra_json, created_at
            """;

    private final JdbcTemplate jdbc;

    public IntentRecordRepository(JdbcTemplate jdbc) {
        this.jdbc = jdbc;
    }

    /**
     * 批量插入。
     *
     * <p>用 {@code batchUpdate} 而不是循环单条插：一次网络往返写多行。
     * 在本项目的数据量下（一局 2 条）这个优化基本无所谓，
     * <b>但批量写的语义更贴合调用方 —— 它本来就是攒一批再刷。</b>
     */
    public void insertBatch(List<IntentRecord> records) {
        if (records == null || records.isEmpty()) {
            return;
        }
        jdbc.batchUpdate(INSERT_SQL, records, records.size(), (ps, r) -> {
            FingerprintInput in = r.input();
            int i = 0;
            ps.setString(++i, r.runId());
            ps.setInt(++i, in.floorIndex());
            ps.setInt(++i, r.schemaVersion());
            ps.setInt(++i, in.challengeBudget());
            ps.setDouble(++i, in.buildConcentration());
            ps.setDouble(++i, in.combatEfficiency());
            ps.setDouble(++i, in.strategySwitch());
            ps.setDouble(++i, in.survivalPressure());
            ps.setDouble(++i, in.confidence());
            ps.setString(++i, in.dominantArchetype());
            ps.setString(++i, in.availableRulesKey());
            ps.setString(++i, in.availableArchKey());
            ps.setString(++i, in.historyTagsKey());
            ps.setString(++i, r.fingerprint());
            ps.setString(++i, r.cacheOutcome().name());
            ps.setInt(++i, r.variantCount());
            ps.setString(++i, r.source());
            ps.setInt(++i, r.httpStatus());
            ps.setInt(++i, r.latencyMs());
            ps.setString(++i, r.profileExtraJson());
            ps.setTimestamp(++i, Timestamp.from(r.createdAt()));
        });
    }

    /**
     * 按时间范围读出全部流水，供模拟器重放。
     *
     * <p><b>按 {@code created_at} 升序</b> —— 模拟器要按真实发生顺序重放缓存状态的演进，
     * 顺序错了算出来的命中率就没有意义。
     *
     * <p>刻意不分页：本表的数据量是「几百行」级别（一局 2 条）。
     * <b>为一个不存在的规模加分页，是把简单问题变复杂</b> ——
     * 真到了需要分页的量级，这个方法会先在内存上暴露问题，那时再改不迟。
     */
    public List<IntentRecord> findBetweenOrderByTime(Instant from, Instant to) {
        return jdbc.query(
                "SELECT " + SELECT_COLUMNS
                        + " FROM intent_request WHERE created_at >= ? AND created_at < ?"
                        + " ORDER BY created_at ASC, id ASC",
                ROW_MAPPER, Timestamp.from(from), Timestamp.from(to));
    }

    public long count() {
        Long n = jdbc.queryForObject("SELECT COUNT(*) FROM intent_request", Long.class);
        return n == null ? 0L : n;
    }

    /** 探测表在不在、连接通不通。启动时用它决定要不要打那条醒目的 WARN。 */
    public void probe() {
        jdbc.queryForObject("SELECT 1", Integer.class);
        jdbc.queryForObject("SELECT COUNT(*) FROM intent_request", Long.class);
    }

    private static final RowMapper<IntentRecord> ROW_MAPPER = (rs, rowNum) -> new IntentRecord(
            rs.getString("run_id"),
            rs.getInt("schema_version"),
            new FingerprintInput(
                    rs.getInt("floor_index"),
                    rs.getInt("challenge_budget"),
                    rs.getDouble("build_concentration"),
                    rs.getDouble("combat_efficiency"),
                    rs.getDouble("strategy_switch"),
                    rs.getDouble("survival_pressure"),
                    rs.getDouble("confidence"),
                    rs.getString("dominant_archetype"),
                    rs.getString("available_rules_key"),
                    rs.getString("available_arch_key"),
                    rs.getString("history_tags_key")),
            rs.getString("fingerprint"),
            CacheOutcome.valueOf(rs.getString("cache_outcome")),
            rs.getInt("variant_count"),
            rs.getString("source"),
            rs.getInt("http_status"),
            rs.getInt("latency_ms"),
            rs.getString("profile_extra_json"),
            rs.getTimestamp("created_at").toInstant());
}
