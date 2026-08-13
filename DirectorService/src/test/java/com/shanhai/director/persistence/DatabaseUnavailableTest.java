package com.shanhai.director.persistence;

import static org.assertj.core.api.Assertions.assertThat;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.web.servlet.AutoConfigureMockMvc;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.TestPropertySource;
import org.springframework.test.web.servlet.MockMvc;

import com.shanhai.director.api.IntentController;

/**
 * ⭐ D-24 硬约束① 的守卫：<b>数据库连不上时，服务端必须照常返回决策，只是不落库。</b>
 *
 * <h2>这条为什么必须有一个测试</h2>
 *
 * 它守的不是某个函数的返回值，而是<b>一条不变量</b>：落库是尽力而为的旁路，
 * 它的失败不能反过来伤害被观测的系统。
 *
 * <p>而这条不变量<b>极其容易在无意中被破坏</b> —— 只要有人把
 * {@code spring.sql.init.mode} 改回 {@code always}、或者给某个 Bean 加一个
 * 数据库依赖并让它在启动期就用，整个上下文就起不来了。
 * <b>那种破坏在有数据库的开发机上完全看不出来。</b>
 *
 * <p>这次就是这么踩到的：先用了 {@code spring.sql.init.mode=always}，
 * 结果所有既有的 {@code @SpringBootTest} 全部无法加载上下文。
 *
 * <h2>为什么指向一个不存在的端口而不是关掉数据源</h2>
 *
 * 关掉数据源测的是「没有这个功能时能不能起」，
 * <b>而真实的故障形态是「配置在、但连不上」</b> —— 后者才会触发连接超时、
 * 触发初始化器、触发所有那些会拖垮启动的路径。
 */
@SpringBootTest
@AutoConfigureMockMvc
@TestPropertySource(properties = {
        // 一个必然连不上的地址。1 端口不会有 MySQL 在听。
        "spring.datasource.url=jdbc:mysql://127.0.0.1:1/nope?connectTimeout=500",
        "spring.datasource.username=nobody",
        "spring.datasource.password=nothing",
        // 连接池不要为了建连接卡住启动
        "spring.datasource.hikari.initialization-fail-timeout=-1",
        "spring.datasource.hikari.connection-timeout=1000",
        "shm.llm.api-key="
})
class DatabaseUnavailableTest {

    @Autowired
    private MockMvc mockMvc;

    @Autowired
    private DatabaseBootstrap bootstrap;

    @Test
    @DisplayName("数据库连不上时，上下文照样加载、决策端点照样工作")
    void decisionEndpointStillWorks_whenDatabaseIsDown() throws Exception {
        // 能跑到这里本身就是最重要的断言：上下文加载成功了。
        assertThat(bootstrap.isAvailable())
                .as("数据库连不上时 DatabaseBootstrap 必须如实报告不可用")
                .isFalse();

        mockMvc.perform(post(IntentController.INTENT_PATH)
                        .contentType(MediaType.APPLICATION_JSON)
                        .content("""
                                {
                                  "schemaVersion": 1,
                                  "runId": "run-db-down",
                                  "floorIndex": 1,
                                  "totalFloors": 3,
                                  "challengeBudget": 30,
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
                                """))
                .andExpect(status().isOk());
    }
}
