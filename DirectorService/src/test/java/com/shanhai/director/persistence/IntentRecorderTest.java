package com.shanhai.director.persistence;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatCode;

import java.time.Instant;
import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import com.shanhai.director.api.IntentRequest;
import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.FingerprintInput;
import com.shanhai.director.cache.FingerprintScheme;

/**
 * {@link IntentRecorder} 的单元测试（M5，决策 D-24）。
 *
 * <p>这里验的是<b>旁路的容错行为</b>，不接数据库 —— 用假的仓库把各种失败造出来。
 * 真的写进数据库由 {@link IntentRecordRepositoryTest} 和 {@link PersistenceEndToEndTest} 验。
 *
 * <p>⭐ 为什么这些必须有测试：<b>这些路径只在出故障时才会走到，
 * 而出故障时没人有空验证「容错本身对不对」。</b>
 */
class IntentRecorderTest {

    /**
     * 硬约束③的前半：<b>队列有界</b>。
     *
     * <p>不测「满了会不会阻塞」而是测「满了会不会丢」——
     * 因为阻塞才是真正的危险：它会把落库的延迟变成玩家的等待。
     */
    @Test
    @DisplayName("队列满了直接丢，绝不阻塞调用方")
    void queueFull_dropsWithoutBlocking() {
        // 用一个永远不消费的 recorder：不调 start()，消费线程就没起来
        IntentRecorder recorder = new IntentRecorder(new NeverCalledRepository(), alwaysAvailable());

        // 塞满 + 多塞 50 条
        int overflow = 50;
        for (int i = 0; i < IntentRecorder.QUEUE_CAPACITY + overflow; i++) {
            final int n = i;
            assertThatCode(() -> recorder.record(sampleRecord(Instant.now().plusMillis(n))))
                    .as("第 %d 条不该抛异常", n)
                    .doesNotThrowAnyException();
        }

        assertThat(recorder.queueDepth()).isEqualTo(IntentRecorder.QUEUE_CAPACITY);
        assertThat(recorder.droppedCount())
                .as("超出容量的部分必须被计入丢弃——一个不可观测的丢弃等于数据缺了没人知道")
                .isEqualTo(overflow);
    }

    /**
     * 硬约束①的一半：<b>数据库不可用时不往队列里攒。</b>
     *
     * <p>攒满之后开始丢，而丢弃计数会把「数据库没起」这个真原因
     * <b>掩盖成「队列满了」</b>，让排查方向从一开始就被带偏。
     */
    @Test
    @DisplayName("数据库不可用时直接计丢弃，不占队列")
    void databaseUnavailable_doesNotFillQueue() {
        IntentRecorder recorder = new IntentRecorder(new NeverCalledRepository(), neverAvailable());

        for (int i = 0; i < 10; i++) {
            recorder.record(sampleRecord(Instant.now()));
        }

        assertThat(recorder.queueDepth()).isZero();
        assertThat(recorder.droppedCount()).isEqualTo(10);
    }

    /**
     * ⭐ 写库抛异常不能杀死消费线程。
     *
     * <p>杀死了的话，之后所有记录都只会堆积到满然后被丢，
     * <b>而这个故障是渐进的、极难定位的</b> —— 和线程池的 worker 不 catch 异常同理。
     */
    @Test
    @DisplayName("写库连续抛异常，消费线程仍活着并继续处理后续批次")
    void repositoryThrows_consumerSurvives() throws Exception {
        ExplodingRepository repo = new ExplodingRepository(2);   // 前两批炸
        IntentRecorder recorder = new IntentRecorder(repo, alwaysAvailable());
        recorder.start();
        try {
            for (int i = 0; i < 3; i++) {
                recorder.record(sampleRecord(Instant.now()));
                // 分开提交，保证是三个不同的批次
                Thread.sleep(300);
            }
            assertThat(recorder.awaitProcessed(3000)).isTrue();

            assertThat(repo.attempts())
                    .as("消费线程必须活过前两次异常，第三批仍要被尝试")
                    .isEqualTo(3);
            assertThat(recorder.failedCount()).isEqualTo(2);
            assertThat(recorder.recordedCount()).isEqualTo(1);
        } finally {
            recorder.destroy();
        }
    }

    @Test
    @DisplayName("正常路径：提交的记录最终被写进仓库")
    void happyPath_recordsReachRepository() throws Exception {
        CountingRepository repo = new CountingRepository();
        IntentRecorder recorder = new IntentRecorder(repo, alwaysAvailable());
        recorder.start();
        try {
            for (int i = 0; i < 5; i++) {
                recorder.record(sampleRecord(Instant.now()));
            }
            assertThat(recorder.awaitProcessed(3000)).isTrue();

            assertThat(repo.total()).isEqualTo(5);
            assertThat(recorder.recordedCount()).isEqualTo(5);
            assertThat(recorder.droppedCount()).isZero();
            assertThat(recorder.failedCount()).isZero();
        } finally {
            recorder.destroy();
        }
    }

    /**
     * ⭐ 连接类失败要把数据库标记成不可用；数据类失败不要。
     *
     * <p>这条是 M5-2 实测时发现的缺口：把 MySQL 容器停掉之后，
     * 指标上是 {@code failed} 在涨而 {@code dropped} 是 0 ——
     * <b>因为 isAvailable() 只在启动时探测了一次，之后再没变过。</b>
     *
     * <p>后果不是功能错误（主链路一直是安全的），而是<b>指标会把人带偏</b>：
     * 看到 failed 会去查 SQL 或 schema，而真实原因是数据库死了。
     * 这与本项目「『被限流』和『后端挂了』必须可区分」是同一条判断。
     */
    @Test
    @DisplayName("拿不到连接时把数据库标记为不可用，后续记录直接计丢弃")
    void connectionFailure_marksDatabaseUnavailable() throws Exception {
        TrackingBootstrap bootstrap = new TrackingBootstrap();
        IntentRecorder recorder = new IntentRecorder(
                new ConnectionRefusedRepository(), bootstrap);
        recorder.start();
        try {
            recorder.record(sampleRecord(Instant.now()));
            assertThat(recorder.awaitProcessed(3000)).isTrue();

            assertThat(bootstrap.markedUnavailable())
                    .as("拿不到连接必须让后续的 record() 直接丢弃，而不是继续入队白等")
                    .isTrue();

            // 标记之后，新记录不再进队列，而是直接计丢弃
            long droppedBefore = recorder.droppedCount();
            recorder.record(sampleRecord(Instant.now()));
            assertThat(recorder.droppedCount()).isEqualTo(droppedBefore + 1);
            assertThat(recorder.queueDepth()).isZero();
        } finally {
            recorder.destroy();
        }
    }

    @Test
    @DisplayName("数据类失败不标记不可用——数据库还活着，下一批可能就好了")
    void dataFailure_doesNotMarkUnavailable() throws Exception {
        TrackingBootstrap bootstrap = new TrackingBootstrap();
        IntentRecorder recorder = new IntentRecorder(new ExplodingRepository(1), bootstrap);
        recorder.start();
        try {
            recorder.record(sampleRecord(Instant.now()));
            assertThat(recorder.awaitProcessed(3000)).isTrue();

            assertThat(bootstrap.markedUnavailable())
                    .as("IllegalStateException 不是连接问题，不该让整个落库停掉")
                    .isFalse();
        } finally {
            recorder.destroy();
        }
    }

    @Test
    @DisplayName("null 记录被安静忽略，不计任何计数")
    void nullRecord_isIgnored() {
        IntentRecorder recorder = new IntentRecorder(new NeverCalledRepository(), alwaysAvailable());
        recorder.record(null);
        assertThat(recorder.queueDepth()).isZero();
        assertThat(recorder.droppedCount()).isZero();
    }

    // ── 替身 ──

    private static DatabaseBootstrap alwaysAvailable() {
        return new DatabaseBootstrap(null) {
            @Override
            public boolean isAvailable() {
                return true;
            }
        };
    }

    private static DatabaseBootstrap neverAvailable() {
        return new DatabaseBootstrap(null) {
            @Override
            public boolean isAvailable() {
                return false;
            }
        };
    }

    /** 记录 markUnavailable 有没有被调过。 */
    private static class TrackingBootstrap extends DatabaseBootstrap {
        private volatile boolean marked;

        TrackingBootstrap() {
            super(null);
        }

        @Override
        public boolean isAvailable() {
            return !marked;
        }

        @Override
        public void markUnavailable(String reason) {
            marked = true;
        }

        boolean markedUnavailable() {
            return marked;
        }
    }

    /** 模拟「数据库连不上」：抛 Spring 包装过的连接异常。 */
    private static class ConnectionRefusedRepository extends IntentRecordRepository {
        ConnectionRefusedRepository() {
            super(null);
        }

        @Override
        public void insertBatch(List<IntentRecord> records) {
            throw new org.springframework.jdbc.CannotGetJdbcConnectionException(
                    "Failed to obtain JDBC Connection",
                    new java.sql.SQLTransientConnectionException("Connection refused"));
        }
    }

    /** 一次都不该被调用到 —— 被调了说明测试的前提错了。 */
    private static class NeverCalledRepository extends IntentRecordRepository {
        NeverCalledRepository() {
            super(null);
        }

        @Override
        public void insertBatch(List<IntentRecord> records) {
            throw new AssertionError("本用例不该走到真正的写入");
        }
    }

    private static class CountingRepository extends IntentRecordRepository {
        private int total;

        CountingRepository() {
            super(null);
        }

        @Override
        public synchronized void insertBatch(List<IntentRecord> records) {
            total += records.size();
        }

        synchronized int total() {
            return total;
        }
    }

    /** 前 n 批抛异常，之后正常。 */
    private static class ExplodingRepository extends IntentRecordRepository {
        private final int explodeFirst;
        private int attempts;

        ExplodingRepository(int explodeFirst) {
            super(null);
            this.explodeFirst = explodeFirst;
        }

        @Override
        public synchronized void insertBatch(List<IntentRecord> records) {
            attempts++;
            if (attempts <= explodeFirst) {
                throw new IllegalStateException("模拟写库失败 #" + attempts);
            }
        }

        synchronized int attempts() {
            return attempts;
        }
    }

    // ── 测试数据 ──

    private static IntentRecord sampleRecord(Instant at) {
        IntentRequest req = new IntentRequest(
                1, "run-1", 1, 3, 30,
                Map.of(
                        "buildConcentration", 87.0,
                        "combatEfficiency", 40.0,
                        "strategySwitch", 0.0,
                        "survivalPressure", 0.0,
                        "confidence", 0.9,
                        "dominantArchetype", "Archetype.Ranger"),
                List.of(new IntentRequest.AvailableRule("Rule.Ammo", "light", 10, List.of())),
                List.of("Enemy.Grunt"),
                List.of());
        return IntentRecord.of(req,
                FingerprintScheme.CURRENT.compute(FingerprintInput.from(req)),
                CacheOutcome.MISS_EMPTY, 0, "Llm", 200, 1234L, null, at);
    }
}
