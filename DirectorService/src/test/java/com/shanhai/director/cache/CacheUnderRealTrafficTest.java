package com.shanhai.director.cache;

import static org.assertj.core.api.Assertions.assertThat;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import com.shanhai.director.api.DirectorIntent;
import com.shanhai.director.api.IntentRequest;

/**
 * 按**真实游戏流量**验证缓存收益。
 *
 * <h2>这组测试为什么必须存在</h2>
 *
 * M3 最初的验证方式是拿同一个请求连发 12 次——3 次攒满候选、之后全部命中，
 * 看起来非常成功。但<b>那个流量模式和游戏完全不一样</b>：
 *
 * <ul>
 *   <li>一局只发 <b>2 次</b>决策请求（共 3 层，F0 是观察层不走 Provider）</li>
 *   <li>F1 与 F2 的 floorIndex、challengeBudget 都不同 → 天然是两条不同指纹</li>
 *   <li>跨局时画像随实际打法漂移（实测两局真实对局的 F1 分桶就不同）</li>
 * </ul>
 *
 * 结果是用户实际打了 3 把，缓存<b>一次都没命中</b>。
 *
 * <p>教训：<b>验证缓存必须用真实的访问模式，不能用自己编的循环。</b>
 * 循环验证的是"缓存能不能存取"，而缓存真正要回答的是"在我的流量下省不省钱"——
 * 这是两个问题，前者绿不代表后者成立。
 */
class CacheUnderRealTrafficTest {

    /** 一局的两次请求：F1（预算 30）与 F2（预算 55）。 */
    private static List<IntentRequest> oneRun(String runId, double combatEfficiency) {
        return List.of(
                request(runId, 1, 30, combatEfficiency, 0.5, List.of()),
                request(runId, 2, 55, combatEfficiency, 0.7, List.of("Rule.Ammo")));
    }

    private static IntentRequest request(String runId, int floor, int budget,
                                         double combatEfficiency, double confidence,
                                         List<String> historyTags) {
        Map<String, Object> profile = new HashMap<>();
        profile.put("buildConcentration", 90.0);
        profile.put("combatEfficiency", combatEfficiency);
        profile.put("strategySwitch", 10.0);
        profile.put("survivalPressure", 15.0);
        profile.put("confidence", confidence);
        profile.put("dominantArchetype", "Archetype.Ranger");
        return new IntentRequest(1, runId, floor, 3, budget, profile,
                List.of(new IntentRequest.AvailableRule("Rule.Ammo", "medium", 20, List.of())),
                List.of("Enemy.Grunt", "Enemy.Tank"),
                List.of(new IntentRequest.HistoryEntry(0, historyTags)));
    }

    private static DirectorIntent intent(String narration) {
        return new DirectorIntent("Counter", Map.of("Enemy.Grunt", 1.0),
                List.of(new DirectorIntent.RuleIntent("Rule.Ammo", "medium")),
                narration, "reason");
    }

    /** 模拟打 N 局（打法稳定，画像一致），返回 LLM 实际被调用的次数。 */
    private static int simulate(IntentCache cache, int runs) {
        int llmCalls = 0;
        for (int r = 0; r < runs; r++) {
            for (IntentRequest req : oneRun("run-" + r, 90.0)) {
                if (cache.lookup(req).isEmpty()) {
                    llmCalls++;
                    cache.store(req, intent("台词-" + llmCalls));
                }
            }
        }
        return llmCalls;
    }

    @Test
    @DisplayName("打 3 局（打法稳定）应当已经省下调用 —— 用户实测 3 局零命中")
    void threeRunsShouldAlreadySaveCalls() {
        IntentCache cache = new IntentCache(new InMemoryIntentCacheStore());

        int llmCalls = simulate(cache, 3);

        // 3 局共 6 次请求。**打法完全稳定、指纹完全一致**的理想情况下，
        // 缓存至少该省下几次调用；一次都省不下就说明它在这个流量下形同虚设。
        assertThat(llmCalls)
                .as("3 局 6 次请求，缓存应至少省下 1 次调用（实测 0 命中即此处会红）")
                .isLessThan(6);
    }

    @Test
    @DisplayName("打 5 局后命中率应当明显 —— 缓存要真的省钱")
    void hitRatioBecomesMeaningful() {
        IntentCache cache = new IntentCache(new InMemoryIntentCacheStore());
        simulate(cache, 5);

        // 5 局 10 次请求，理想情况（指纹稳定）下应当有相当比例走缓存。
        // 30% 是个保守下限：达不到就说明预热成本吃掉了全部收益。
        assertThat(cache.hitRatio())
                .as("5 局之后命中率应不低于 30%%，否则缓存不值得存在")
                .isGreaterThanOrEqualTo(0.3);
    }

    @Test
    @DisplayName("F1 与 F2 是两条独立指纹 —— 预算不同，决策语境本就不同")
    void floorsAreSeparateFingerprints() {
        IntentCache cache = new IntentCache(new InMemoryIntentCacheStore());
        List<IntentRequest> run = oneRun("run-x", 90.0);

        String fpF1 = cache.fingerprint(run.get(0));
        String fpF2 = cache.fingerprint(run.get(1));
        assertThat(fpF1)
                .as("F1 预算 30、F2 预算 55，是不同的决策语境，不该共用缓存")
                .isNotEqualTo(fpF2);
    }

    @Test
    @DisplayName("台词多样性不能因为提前命中而丢失")
    void varietyStillEmergesOverTime() {
        IntentCache cache = new IntentCache(new InMemoryIntentCacheStore());
        simulate(cache, 20);

        // 打够多局之后，同一指纹下仍应攒到多条候选，
        // 否则就是拿"千人一句"换命中率。
        IntentRequest f1 = oneRun("probe", 90.0).get(0);
        assertThat(cache.variantCount(f1))
                .as("长期来看同一语境应攒到多条台词，不能只有一条")
                .isGreaterThan(1);
    }
}
