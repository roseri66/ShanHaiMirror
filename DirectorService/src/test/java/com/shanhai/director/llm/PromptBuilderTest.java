package com.shanhai.director.llm;

import static org.assertj.core.api.Assertions.assertThat;

import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import com.shanhai.director.api.IntentRequest;

/**
 * prompt 组装的单测 —— 纯函数，不碰网络、不碰 LLM。
 *
 * <p>守的是"服务端产出的 prompt 与客户端 SHMPromptBuilder.cpp 一致"。
 * 这类错不会让任何东西报错，只会让 LLM 看到的信息悄悄变味：
 * 层号差 1、少一行主力打法、漏掉互斥信息——表现都是"决策质量下降"，
 * 而这几乎不可能靠肉眼在游戏里发现。
 */
class PromptBuilderTest {

    private final PromptBuilder builder = new PromptBuilder();

    private static IntentRequest request() {
        return new IntentRequest(
                1, "run", 1, 3, 30,
                Map.of("buildConcentration", 87.0,
                        "combatEfficiency", 72.0,
                        "strategySwitch", 15.0,
                        "survivalPressure", 22.0,
                        "confidence", 0.9,
                        "dominantArchetype", "Archetype.Ranger",
                        "primaryBuildTags", List.of("Build.Ranged")),
                List.of(new IntentRequest.AvailableRule(
                        "Rule.Ammo", "medium", 20, List.of("Rule.RangedDamage"))),
                List.of("Enemy.Grunt", "Enemy.Tank"),
                List.of(new IntentRequest.HistoryEntry(0, List.of("Rule.Cooldown"))));
    }

    @Test
    @DisplayName("层号必须 +1 —— 内部 0-based，给 LLM 看 1-based")
    void floorIndexIsOneBasedForDisplay() {
        // floorIndex=1 且 totalFloors=3 时，C++ 端打印的是「第 2 层 / 共 3 层」。
        // 少加这个 1，LLM 会看到"第 0 层"，对"还剩几层"的判断整体偏移。
        assertThat(builder.buildUserPrompt(request())).contains("【当前进度】第 2 层 / 共 3 层");
    }

    @Test
    @DisplayName("历史条目的层号同样 +1")
    void historyFloorIndexIsOneBased() {
        assertThat(builder.buildUserPrompt(request())).contains("第 1 层: Rule.Cooldown");
    }

    @Test
    @DisplayName("画像五项齐全，且不含 resourceSurplus")
    void profileHasFiveDimensionsWithoutResourceSurplus() {
        String p = builder.buildUserPrompt(request());
        assertThat(p).contains("Build 集中度 : 87");
        assertThat(p).contains("战斗效率     : 72");
        assertThat(p).contains("策略切换意愿 : 15");
        assertThat(p).contains("生存压力     : 22");
        assertThat(p).contains("判断置信度   : 0.90");

        // resourceSurplus 恒为 50（D-09 砍了道具系统，无数据源）。
        // 列进 prompt 等于告诉 LLM 一个假观测——与雷达图不画它是同一条理由。
        assertThat(p).doesNotContain("资源盈余");
        assertThat(p).doesNotContain("resourceSurplus");
    }

    @Test
    @DisplayName("主力打法必须出现 —— 它是上行契约里专门为 prompt 补的字段")
    void primaryBuildTagsAppear() {
        assertThat(builder.buildUserPrompt(request())).contains("主力打法     : Build.Ranged");
    }

    @Test
    @DisplayName("互斥信息必须注入 —— 不给它 LLM 只能盲选")
    void conflictsAreInjected() {
        // 2026-07-28 实测：不注入时 DeepSeek 同时挑了「弹药↓ + 远程伤害↓」，
        // 对远程玩家是无解组合，被客户端 Conflict 护栏拒并白白降级一次。
        assertThat(builder.buildUserPrompt(request()))
                .contains("【不可与以下规则同时选用： Rule.RangedDamage】");
    }

    @Test
    @DisplayName("无可用规则时给出明确指示，而不是留一段空白")
    void emptyRulesGetExplicitInstruction() {
        IntentRequest noRules = new IntentRequest(
                1, "run", 0, 3, 0,
                Map.of("confidence", 0.5),
                List.of(), List.of("Enemy.Grunt"), List.of());
        assertThat(builder.buildUserPrompt(noRules))
                .contains("（本层无可用规则，ruleIntents 请返回空数组）");
    }

    @Test
    @DisplayName("字段缺失不得抛异常 —— 不信任客户端输入是基本功")
    void missingFieldsDoNotThrow() {
        IntentRequest sparse = new IntentRequest(
                1, null, null, null, null, null, null, null, null);
        assertThat(builder.buildUserPrompt(sparse)).contains("请给出这一层的导演决策 JSON。");
    }
}
