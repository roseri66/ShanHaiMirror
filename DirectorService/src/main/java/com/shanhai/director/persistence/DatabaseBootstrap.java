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
}
