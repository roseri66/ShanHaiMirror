package com.shanhai.director.cache;

import static org.assertj.core.api.Assertions.assertThat;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.HashSet;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import com.shanhai.director.api.DirectorIntent;
import com.shanhai.director.api.IntentRequest;

/**
 * 缓存的单测。
 *
 * <p><b>指纹算错不会报错</b>——只会表现为"命中率异常低"（该命中的没命中）
 * 或"不该命中的命中了"（两类玩家共用一份决策）。两者都极难在运行中发现，
 * 所以指纹的每一条规则都单独钉一个用例。
 */
class IntentCacheTest {

    private IntentCache cache;
    private IntentCacheStore store;

    @BeforeEach
    void setUp() {
        store = new InMemoryIntentCacheStore();
        cache = new IntentCache(store);
    }

    private static IntentRequest req(String runId, double buildConc, double confidence,
                                     List<String> archetypes) {
        Map<String, Object> profile = new HashMap<>();
        profile.put("buildConcentration", buildConc);
        profile.put("combatEfficiency", 70.0);
        profile.put("strategySwitch", 15.0);
        profile.put("survivalPressure", 22.0);
        profile.put("confidence", confidence);
        profile.put("dominantArchetype", "Archetype.Ranger");
        return new IntentRequest(1, runId, 1, 3, 30, profile,
                List.of(new IntentRequest.AvailableRule("Rule.Ammo", "medium", 20, List.of())),
                archetypes, List.of());
    }

    private static DirectorIntent intent(String narration) {
        return new DirectorIntent("Counter", Map.of("Enemy.Grunt", 1.0),
                List.of(new DirectorIntent.RuleIntent("Rule.Ammo", "medium")),
                narration, "reason");
    }

    // ---------------------------------------------------------------- 指纹

    @Test
    @DisplayName("runId 不入指纹 —— 入了必然永不命中")
    void runIdDoesNotAffectFingerprint() {
        String a = cache.fingerprint(req("run-1", 87, 0.9, List.of("Enemy.Grunt")));
        String b = cache.fingerprint(req("run-2", 87, 0.9, List.of("Enemy.Grunt")));
        assertThat(a).isEqualTo(b);
    }

    @Test
    @DisplayName("画像分桶：87 分与 85 分算同一类玩家")
    void nearbyProfileValuesShareBucket() {
        String a = cache.fingerprint(req("r", 87, 0.9, List.of("Enemy.Grunt")));
        String b = cache.fingerprint(req("r", 85, 0.9, List.of("Enemy.Grunt")));
        assertThat(a).as("同一 20 分桶内应指纹相同").isEqualTo(b);
    }

    @Test
    @DisplayName("跨桶必须区分：87 分与 45 分不是同一类玩家")
    void distantProfileValuesDiffer() {
        String a = cache.fingerprint(req("r", 87, 0.9, List.of("Enemy.Grunt")));
        String b = cache.fingerprint(req("r", 45, 0.9, List.of("Enemy.Grunt")));
        assertThat(a).isNotEqualTo(b);
    }

    @Test
    @DisplayName("置信度三档：0.71 与 0.79 同档，0.5 与 0.9 不同档")
    void confidenceIsTiered() {
        // 护栏只在 0.6 处有阈值行为，分得更细是假精度
        assertThat(cache.fingerprint(req("r", 87, 0.71, List.of("Enemy.Grunt"))))
                .isEqualTo(cache.fingerprint(req("r", 87, 0.79, List.of("Enemy.Grunt"))));

        Set<String> tiers = new HashSet<>();
        tiers.add(cache.fingerprint(req("r", 87, 0.50, List.of("Enemy.Grunt"))));
        tiers.add(cache.fingerprint(req("r", 87, 0.70, List.of("Enemy.Grunt"))));
        tiers.add(cache.fingerprint(req("r", 87, 0.90, List.of("Enemy.Grunt"))));
        assertThat(tiers).as("低/中/高三档应互不相同").hasSize(3);
    }

    @Test
    @DisplayName("候选集是集合语义：顺序不同不该产生不同指纹")
    void archetypeOrderDoesNotMatter() {
        // 不排序的话，同一组候选换个顺序就是另一条指纹，
        // 命中率会莫名其妙地低，而且极难发现原因
        String a = cache.fingerprint(req("r", 87, 0.9, List.of("Enemy.Grunt", "Enemy.Tank")));
        String b = cache.fingerprint(req("r", 87, 0.9, List.of("Enemy.Tank", "Enemy.Grunt")));
        assertThat(a).isEqualTo(b);
    }

    @Test
    @DisplayName("候选集内容不同必须区分 —— 否则会发出选不了的规则")
    void differentArchetypesDiffer() {
        String a = cache.fingerprint(req("r", 87, 0.9, List.of("Enemy.Grunt")));
        String b = cache.fingerprint(req("r", 87, 0.9, List.of("Enemy.Tank")));
        assertThat(a).isNotEqualTo(b);
    }

    // ---------------------------------------------------------------- 轮询

    @Test
    @DisplayName("预热期边用边攒：有候选就能命中，但隔一次仍补充")
    void warmsUpWhileServing() {
        // 最初的规则是"未满 3 条一律不命中"，在真实流量下等于缓存永不生效——
        // 一局只发 2 次决策请求，同一指纹要攒满 3 次得连打 4 局以上。
        // 用户实测打 3 把零命中，故改成边用边攒。
        IntentRequest r = req("run", 87, 0.9, List.of("Enemy.Grunt"));

        assertThat(cache.lookup(r)).as("一条候选都没有时只能走 LLM").isEmpty();
        cache.store(r, intent("第一句"));

        // 有候选之后：交替——偶数次查询走缓存，奇数次继续补充
        assertThat(cache.lookup(r)).as("第 1 次查询（奇数）继续补充").isEmpty();
        assertThat(cache.lookup(r)).as("第 2 次查询（偶数）应命中").isPresent();
    }

    @Test
    @DisplayName("攒满 3 条后进入稳态，每次都命中")
    void alwaysHitsOnceFull() {
        IntentRequest r = req("run", 87, 0.9, List.of("Enemy.Grunt"));
        cache.store(r, intent("一"));
        cache.store(r, intent("二"));
        cache.store(r, intent("三"));

        for (int i = 0; i < 10; i++) {
            assertThat(cache.lookup(r)).as("稳态下第 %d 次应命中", i + 1).isPresent();
        }
    }

    @Test
    @DisplayName("命中时在 3 条里随机取 —— 同类玩家不该听到同一句台词")
    void hitsRotateAmongVariants() {
        IntentRequest r = req("run", 87, 0.9, List.of("Enemy.Grunt"));
        cache.store(r, intent("第一句"));
        cache.store(r, intent("第二句"));
        cache.store(r, intent("第三句"));

        Set<String> seen = new HashSet<>();
        for (int i = 0; i < 200; i++) {
            cache.lookup(r).ifPresent(x -> seen.add(x.narration()));
        }
        // 200 次里只取到 1 种的概率是 (1/3)^199，实际为零
        assertThat(seen).as("应能取到全部三种说法").hasSize(3);
    }

    @Test
    @DisplayName("超过 3 条不再追加 —— 否则内存无界增长")
    void variantsAreCappedAtThree() {
        IntentRequest r = req("run", 87, 0.9, List.of("Enemy.Grunt"));
        for (int i = 0; i < 10; i++) {
            cache.store(r, intent("句子" + i));
        }
        assertThat(store.get(cache.fingerprint(r))).hasSize(IntentCache.MAX_VARIANTS);
    }

    @Test
    @DisplayName("不同画像各自独立缓存")
    void differentProfilesCacheSeparately() {
        IntentRequest ranger = req("run", 87, 0.9, List.of("Enemy.Grunt"));
        IntentRequest weak = req("run", 20, 0.5, List.of("Enemy.Grunt"));

        for (int i = 0; i < 3; i++) {
            cache.store(ranger, intent("远程玩家的台词"));
        }
        assertThat(cache.lookup(ranger)).isPresent();
        assertThat(cache.lookup(weak)).as("另一类画像不该命中别人的缓存").isEmpty();
    }

    @Test
    @DisplayName("命中率统计正确 —— M4 的指标依赖它")
    void hitRatioIsTracked() {
        IntentRequest r = req("run", 87, 0.9, List.of("Enemy.Grunt"));

        // 先攒满，进入稳态
        cache.store(r, intent("一"));
        cache.store(r, intent("二"));
        cache.store(r, intent("三"));

        for (int i = 0; i < 7; i++) {
            Optional<DirectorIntent> hit = cache.lookup(r);
            assertThat(hit).isPresent();
        }
        assertThat(cache.hitCount()).isEqualTo(7);
        assertThat(cache.missCount()).isZero();
        assertThat(cache.hitRatio()).isEqualTo(1.0);
    }
}
