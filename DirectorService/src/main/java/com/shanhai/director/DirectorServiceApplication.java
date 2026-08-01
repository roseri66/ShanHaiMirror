package com.shanhai.director;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

/**
 * 山海镜决策网关（DECISIONS D-23）。
 *
 * <p>这个服务存在的唯一理由是两条：
 * <ol>
 *   <li><b>key 不能进客户端</b> —— 此前 SHM_LLM_API_KEY 是客户端环境变量，
 *       任何拿到构建版的人都能读走</li>
 *   <li><b>决策数据要能跨局聚合</b> —— 决策日志此前只存在各自的 Saved/ 里</li>
 * </ol>
 * 不服务于这两条的功能一律不做：没有账号、没有排行榜、没有管理后台、
 * 不拆微服务。
 *
 * <p><b>四道护栏不在这里。</b>它们留在客户端，这是本设计最重要的一条否决：
 * 护栏搬到服务端会破坏"断网可玩"这条不变量（后端不可达时就没人校验本地
 * Provider 的输出了），而且 C++/Java 双写必然漂移。
 * 单机游戏里客户端本来就是权威，为一个不成立的信任边界付双写代价是纯亏。
 */
@SpringBootApplication
public class DirectorServiceApplication {

    public static void main(String[] args) {
        SpringApplication.run(DirectorServiceApplication.class, args);
    }
}
