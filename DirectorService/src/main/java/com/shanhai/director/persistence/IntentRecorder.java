package com.shanhai.director.persistence;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.DisposableBean;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

/**
 * 把决策流水异步写进数据库（M5，决策 D-24）。
 *
 * <h2>⭐ 三条硬约束，任何改动都必须满足</h2>
 *
 * <ol>
 *   <li><b>数据库挂了 → 服务端照常返回决策，只是不落库</b></li>
 *   <li><b>落库耗时不计入决策响应时间</b></li>
 *   <li><b>队列必须有界，且丢弃必须可观测</b></li>
 * </ol>
 *
 * <p>理由与「上游失败返 5xx 而不悄悄回落 stub」同源：那条是为了不往证据链里掺假，
 * <b>这条是为了不让证据的采集反过来伤害被观测的系统。</b>
 *
 * <h2>为什么队列要有界</h2>
 *
 * 无界队列在数据库慢或挂掉时会一直涨，直到把堆吃光 ——
 * <b>那时故障从「不落库」升级成「整个服务 OOM」，而起因只是一个旁路功能。</b>
 * 有界队列把最坏情况钉死在一个常量上。
 *
 * <p>代价是满了要丢。<b>丢是设计内的行为，但必须可见</b> ——
 * {@code shm.persist.dropped} 这条指标不能省。这与本项目「限流器被抽干不算降级，
 * 但要看指标」是同一条判断：<b>一个正确的丢弃，如果不可观测，
 * 就等于数据缺了都没人知道。</b>
 *
 * <h2>为什么用带超时的 poll 而不是 take</h2>
 *
 * {@code take()} 在队列空时会无限阻塞，停机时消费线程醒不过来。
 * 带超时的 {@code poll} 让它每 200ms 有机会检查停机标志，
 * <b>也让「攒不满一批也要刷」成为可能</b> —— 否则最后几条会一直卡在队列里，
 * 人工验收时会以为没写进去。
 *
 * @since M5
 */
@Component
public class IntentRecorder implements DisposableBean {

    private static final Logger log = LoggerFactory.getLogger(IntentRecorder.class);

    /**
     * 队列容量。
     *
     * <p>一局 2 条，1000 条约等于 500 局的缓冲 —— <b>远超任何真实积压</b>。
     * 这个数字的意义不在于「够用」，在于<b>把最坏情况钉死</b>：
     * 无论数据库挂多久，内存占用有上界。
     */
    static final int QUEUE_CAPACITY = 1000;

    /** 批量大小。本项目数据量小，批量的意义主要是减少往返而不是吞吐。 */
    static final int BATCH_SIZE = 50;

    /** 攒不满一批时的最长等待。见类注释：不设的话最后几条会卡在队列里。 */
    private static final long POLL_TIMEOUT_MS = 200;

    private final BlockingQueue<IntentRecord> queue = new ArrayBlockingQueue<>(QUEUE_CAPACITY);
    private final IntentRecordRepository repository;
    private final DatabaseBootstrap bootstrap;

    private final AtomicLong recorded = new AtomicLong();
    private final AtomicLong dropped = new AtomicLong();
    private final AtomicLong failed = new AtomicLong();

    private volatile boolean running = true;
    private Thread consumer;

    public IntentRecorder(IntentRecordRepository repository, DatabaseBootstrap bootstrap) {
        this.repository = repository;
        this.bootstrap = bootstrap;
    }

    /**
     * 应用就绪后才起消费线程。
     *
     * <p>顺序是刻意的：{@link DatabaseBootstrap} 也监听同一个事件建表，
     * 而 Spring 按 Bean 名字排序调用同一事件的监听器 ——
     * <b>不能依赖那个顺序</b>。所以消费线程每次刷盘前都自己查
     * {@link DatabaseBootstrap#isAvailable()}，而不是假设建表已完成。
     */
    @EventListener(ApplicationReadyEvent.class)
    public void start() {
        if (consumer != null) {
            return;
        }
        consumer = new Thread(this::consumeLoop, "intent-recorder");
        // 守护线程：它不该阻止 JVM 退出。停机时的收尾由 destroy() 负责。
        consumer.setDaemon(true);
        consumer.start();
    }

    /**
     * 提交一条流水。<b>非阻塞</b> —— 队列满了直接丢，绝不阻塞调用方。
     *
     * <p>调用方在决策请求的响应路径上，<b>任何阻塞都会变成玩家的等待</b>。
     */
    public void record(IntentRecord record) {
        if (record == null) {
            return;
        }
        // 数据库都不可用就别往队列里攒了 ——
        // 攒满之后开始丢，而丢弃计数会把「数据库没起」这个真原因
        // 掩盖成「队列满了」，让排查方向从一开始就被带偏。
        if (!bootstrap.isAvailable()) {
            dropped.incrementAndGet();
            return;
        }
        if (!queue.offer(record)) {
            long n = dropped.incrementAndGet();
            // 每 100 条丢弃打一条 WARN：既不刷屏，又不至于完全静默。
            if (n % 100 == 1) {
                log.warn("[M5] 落库队列已满（容量 {}），累计丢弃 {} 条。"
                        + "服务本身不受影响，但这段时间的数据不会进入聚合分析。",
                        QUEUE_CAPACITY, n);
            }
        }
    }

    private void consumeLoop() {
        List<IntentRecord> batch = new ArrayList<>(BATCH_SIZE);
        while (running || !queue.isEmpty()) {
            try {
                IntentRecord first = queue.poll(POLL_TIMEOUT_MS, TimeUnit.MILLISECONDS);
                if (first == null) {
                    continue;
                }
                batch.add(first);
                queue.drainTo(batch, BATCH_SIZE - 1);
                flush(batch);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();   // 恢复中断标志
                break;
            } catch (RuntimeException e) {
                // ⭐ 单批失败绝不能杀死消费线程，否则之后所有记录都只会堆积到满然后被丢，
                //    而且这个故障是渐进的、极难定位的（和线程池 worker 不 catch 异常同理）。
                failed.addAndGet(batch.size());
                log.warn("[M5] 落库失败，丢弃本批 {} 条：{}", batch.size(), e.toString());
                log.debug("[M5] 落库失败的完整堆栈", e);
                batch.clear();

                // ⭐ 区分「拿不到连接」和「这批数据有问题」——两者的应对完全不同：
                //    前者说明数据库没了，继续尝试只是每条都白等一次连接超时；
                //    后者是数据或 schema 的问题，数据库还活着，下一批可能就好了。
                //    不区分的话，指标上只会看到 failed 一路涨，而看不出到底该去查什么。
                if (isConnectionFailure(e)) {
                    bootstrap.markUnavailable(e.getClass().getSimpleName());
                }
            }
        }
        // 停机收尾：把队列里剩的刷完
        if (!queue.isEmpty()) {
            queue.drainTo(batch);
            try {
                flush(batch);
            } catch (RuntimeException e) {
                log.warn("[M5] 停机时的最后一批落库失败，{} 条丢弃：{}", batch.size(), e.toString());
            }
        }
    }

    /**
     * 这次失败是不是「根本连不上数据库」。
     *
     * <p>顺着 cause 链找 —— Spring 会把底层的 {@code SQLException} 包好几层，
     * 只看最外层那个类型是判不出来的。
     */
    private static boolean isConnectionFailure(Throwable e) {
        for (Throwable t = e; t != null; t = t.getCause()) {
            if (t instanceof org.springframework.jdbc.CannotGetJdbcConnectionException
                    || t instanceof java.sql.SQLTransientConnectionException
                    || t instanceof java.sql.SQLNonTransientConnectionException) {
                return true;
            }
            if (t.getCause() == t) {
                break;   // 自引用的 cause 链，防死循环
            }
        }
        return false;
    }

    private void flush(List<IntentRecord> batch) {
        if (batch.isEmpty()) {
            return;
        }
        if (!bootstrap.isAvailable()) {
            dropped.addAndGet(batch.size());
            batch.clear();
            return;
        }
        repository.insertBatch(batch);
        recorded.addAndGet(batch.size());
        log.debug("[M5] 落库 {} 条", batch.size());
        batch.clear();
    }

    /**
     * 优雅停机：让消费线程把队列里剩的刷完再退出。
     *
     * <p>不 interrupt —— 中断会打断正在进行的那一批。给它一个有界的等待，
     * <b>超时就放弃，绝不让一个旁路功能拖住整个进程的退出。</b>
     */
    @Override
    public void destroy() throws InterruptedException {
        running = false;
        if (consumer != null) {
            consumer.join(TimeUnit.SECONDS.toMillis(3));
        }
    }

    /** 测试用：等队列被消费干净。返回是否在超时内完成。 */
    boolean awaitDrained(long timeoutMs) throws InterruptedException {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (System.currentTimeMillis() < deadline) {
            if (queue.isEmpty()) {
                // 队列空了不代表最后一批已经写进去，再给一个 poll 周期
                Thread.sleep(POLL_TIMEOUT_MS + 100);
                return queue.isEmpty();
            }
            Thread.sleep(20);
        }
        return false;
    }

    public long recordedCount() {
        return recorded.get();
    }

    public long droppedCount() {
        return dropped.get();
    }

    public long failedCount() {
        return failed.get();
    }

    public int queueDepth() {
        return queue.size();
    }
}
