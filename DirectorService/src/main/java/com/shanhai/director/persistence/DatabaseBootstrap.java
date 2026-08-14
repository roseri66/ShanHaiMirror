package com.shanhai.director.persistence;

import java.nio.charset.StandardCharsets;

import javax.sql.DataSource;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.context.event.ApplicationReadyEvent;
import org.springframework.context.event.EventListener;
import org.springframework.core.io.ClassPathResource;
import org.springframework.jdbc.datasource.init.ResourceDatabasePopulator;
import org.springframework.stereotype.Component;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * 建表 + 启动探测（M5，决策 D-24）。
 *
 * <h2>⭐ 为什么不用 Spring 自带的 {@code spring.sql.init}</h2>
 *
 * 试过，<b>它违反 D-24 的硬约束①</b>：{@code spring.sql.init.mode=always} 会让
 * {@code dataSourceScriptDatabaseInitializer} 成为一个启动期的<b>硬依赖</b> ——
 * 连不上数据库时整个 ApplicationContext 加载失败，服务根本起不来。
 *
 * <p>而 D-24 明确要求：<b>MySQL 挂了 / 连不上时，服务端必须照常返回决策，只是不落库。</b>
 * 落库是尽力而为的旁路 —— <b>让证据的采集反过来伤害被观测的系统，是本末倒置。</b>
 *
 * <p>{@code continue-on-error} 救不了这个：它只在「脚本执行出错」时继续，
 * <b>连不上是「拿不到连接」，在它的兜底范围之外。</b>
 *
 * <h2>⭐ 但降级必须出声</h2>
 *
 * 探测失败时打一条<b>刻意醒目</b>的 WARN。理由是本项目的一条既有纪律：
 * <b>静默失败会让「没生效」和「没执行」无法区分</b> ——
 * 在这里就是让「没落库」和「落了但查不到」无法区分，而这两者的排查方向完全相反。
 *
 * @since M5
 */
@Component
public class DatabaseBootstrap {

    private static final Logger log = LoggerFactory.getLogger(DatabaseBootstrap.class);

    private final DataSource dataSource;
    private final AtomicBoolean available = new AtomicBoolean(false);

    public DatabaseBootstrap(DataSource dataSource) {
        this.dataSource = dataSource;
    }

    /**
     * 应用就绪后建表并探测。
     *
     * <p>用 {@link ApplicationReadyEvent} 而不是 {@code @PostConstruct}：
     * 后者发生在上下文构建期，<b>那时抛出的异常仍然会让启动失败</b> ——
     * 而这正是我们要避开的。就绪事件的监听器抛异常不影响服务已经起好的事实。
     */
    @EventListener(ApplicationReadyEvent.class)
    public void initialize() {
        try {
            // 分隔符用默认的 ";" —— schema.sql 里只有一条 CREATE TABLE，
            // 且它用的是 "--" 行注释，不会和分隔符打架。
            new ResourceDatabasePopulator(new ClassPathResource("schema.sql")).execute(dataSource);
            available.set(true);
            log.info("[M5] 决策流水表已就绪，落库已启用。");
        } catch (RuntimeException e) {
            available.set(false);
            // 刻意用多行方框：这条信息一旦被淹没在启动日志里，
            // 人就会以为在落库而实际没有 —— 那比不落库本身糟得多。
            log.warn("""

                    ╔══════════════════════════════════════════════════════════════════╗
                    ║  ⚠️  数据库不可用，决策流水【不会落库】。                          ║
                    ║                                                                  ║
                    ║  服务本身照常工作 —— 决策、缓存、限流、熔断全都不受影响。          ║
                    ║  受影响的只有 M5 的数据回流：这段时间的请求不会进入聚合分析。      ║
                    ║                                                                  ║
                    ║  要启用落库，起一个 MySQL 再重启本服务：                          ║
                    ║    docker run -d --name shm-mysql -p 3306:3306 \\                 ║
                    ║      -e MYSQL_ROOT_PASSWORD=root -e MYSQL_DATABASE=shm_director \\║
                    ║      mysql:8.0                                                   ║
                    ╚══════════════════════════════════════════════════════════════════╝
                    原因：{}""", e.getMessage());
            log.debug("[M5] 建表失败的完整堆栈", e);
        }
    }

    /**
     * 落库当前可不可用。
     *
     * <p>{@code IntentRecorder} 用它决定要不要往队列里放 —— 明知写不进去还攒着，
     * 只会让队列涨到满然后开始丢，而丢弃计数会把「数据库没起」这个真原因掩盖成「队列满了」。
     */
    public boolean isAvailable() {
        return available.get();
    }

    /**
     * 标记数据库已不可用。由 {@code IntentRecorder} 在<b>拿不到连接</b>时调用。
     *
     * <h2>为什么需要它</h2>
     *
     * {@link #initialize()} 只在启动时探测一次。<b>服务起来之后数据库挂掉，这个标志不会变</b> ——
     * 于是 recorder 会继续入队、继续尝试写、继续失败。
     *
     * <p>实测过这个场景（M5-2 验收时把容器停掉）：指标显示的是
     * {@code shm_persist_failed} 在涨，而 {@code shm_persist_dropped} 是 0。
     * <b>这会把人带偏</b> —— 看到 failed 会去查 SQL 或 schema，
     * 而真实原因是数据库死了。这与本项目「『被限流』和『后端挂了』必须可区分」是同一条判断。
     *
     * <h2>⚠️ 刻意不做自动恢复</h2>
     *
     * 置回 {@code true} 只发生在<b>重启服务</b>时。理由：自动恢复要引入「多久重探一次」
     * 这个新参数，而它又是一个拍脑袋的值 —— <b>M5 这一版正是在处理「拍脑袋的参数」，
     * 不该顺手再制造一个。</b>
     *
     * <p>代价是数据库恢复后要重启服务才恢复落库。对一个本机跑、不部署公网的服务，
     * 这个代价可以接受，而且启动日志会明确说明当前状态。
     *
     * @param reason 供日志用的原因，方便事后区分是哪一类失败导致的
     */
    public void markUnavailable(String reason) {
        if (available.compareAndSet(true, false)) {
            log.warn("[M5] 数据库连接已失效，落库停止。原因：{}。"
                    + "服务本身不受影响；恢复数据库后需重启本服务才会重新落库。", reason);
        }
    }
}
