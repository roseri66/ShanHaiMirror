package com.shanhai.director.cache;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import java.util.EnumSet;
import java.util.List;
import java.util.Map;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import com.shanhai.director.api.IntentRequest;

/**
 * {@link FingerprintScheme} 的测试（M5，决策 D-24）。
 *
 * <p>这些用例守的是三件事：
 * <ol>
 *   <li><b>M5 的重构没有改变线上指纹的任何一个字节</b> —— 既有的缓存测试只断言
 *       「两条指纹相不相等」，即便格式整个变了也照样绿。要证明「逐字节一致」，
 *       只能钉字面量。</li>
 *   <li>字段裁剪真的生效 —— 模拟器的全部意义就建立在这上面。</li>
 *   <li>非法方案在构造期就失败，而不是在报告里给出一个诱人的假数字。</li>
 * </ol>
 */
class FingerprintSchemeTest {

    /**
     * ⭐ 把线上指纹的字面量钉死。
     *
     * <p>M3 的原实现拼出来就是这个格式。M5 把算法搬到了 {@link FingerprintScheme}，
     * <b>这条用例是「搬家没搬坏」的唯一硬证据</b>：既有的 11 条缓存测试全都只比较
     * 指纹之间的相等关系，格式整体改掉它们一条都不会红。
     *
     * <p>⚠️ 将来若有人有意改动指纹格式，这条会红 —— <b>那时该做的是想清楚代价再改这条断言</b>
     * （改格式等于让线上已攒的缓存全部失效），而不是顺手把它删掉。
     */
    @Test
    @DisplayName("CURRENT 方案的输出与 M3 原实现逐字节一致")
    void current_producesExactLegacyFormat() {
        String fp = FingerprintScheme.CURRENT.compute(FingerprintInput.from(sample()));

        assertThat(fp).isEqualTo(
                "f=1|b=30|bu=4|co=2|st=0|su=0|c=hi|a=Archetype.Ranger"
                        + "|r=Rule.Ammo:light|e=Enemy.Grunt|h=-");
    }

    @Test
    @DisplayName("去掉 BUDGET 之后，指纹里就没有 b= 这一段了")
    void without_dropsTheSegment() {
        FingerprintScheme noBudget = FingerprintScheme.CURRENT.without(FingerprintScheme.Field.BUDGET);

        String fp = noBudget.compute(FingerprintInput.from(sample()));

        assertThat(fp).doesNotContain("b=30");
        assertThat(fp).startsWith("f=1|bu=");
        // 其余字段一个不少：裁剪必须是外科手术式的，不能顺手动别的
        assertThat(fp).contains("c=hi", "a=Archetype.Ranger", "r=Rule.Ammo:light", "h=-");
    }

    /**
     * 桶宽变了，落在不同桶里的两个画像才可能合并成同一条指纹。
     *
     * <p>87 分与 65 分（整数除法）：
     * <ul>
     *   <li>桶宽 20：{@code 87/20 = 4}、{@code 65/20 = 3} —— 两条指纹</li>
     *   <li>桶宽 50：{@code 87/50 = 1}、{@code 65/50 = 1} —— 同一条</li>
     * </ul>
     */
    @Test
    @DisplayName("加宽桶宽会把原本分开的两个画像并进同一条指纹")
    void widerBucket_mergesFingerprints() {
        FingerprintInput a = FingerprintInput.from(sampleWithBuildConcentration(87));
        FingerprintInput b = FingerprintInput.from(sampleWithBuildConcentration(65));

        assertThat(FingerprintScheme.CURRENT.compute(a))
                .isNotEqualTo(FingerprintScheme.CURRENT.compute(b));

        FingerprintScheme wide = FingerprintScheme.CURRENT.withBucket(50);
        assertThat(wide.compute(a)).isEqualTo(wide.compute(b));
    }

    /**
     * 空字段集会让所有请求算出同一条指纹（空串），命中率恒为 100%。
     *
     * <p><b>那不是一个「更好的方案」，是一个退化成「永远返回上一次结果」的 bug。</b>
     * 让它构造期就炸，免得模拟报告里出现一个诱人的假数字。
     */
    @Test
    @DisplayName("空字段集在构造期就被拒绝")
    void emptyFields_rejectedAtConstruction() {
        assertThatThrownBy(() -> new FingerprintScheme(
                "empty", 20, 3, EnumSet.noneOf(FingerprintScheme.Field.class)))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("空指纹");
    }

    @Test
    @DisplayName("非正的桶宽与候选数在构造期就被拒绝")
    void nonPositiveParams_rejectedAtConstruction() {
        EnumSet<FingerprintScheme.Field> all = EnumSet.allOf(FingerprintScheme.Field.class);

        assertThatThrownBy(() -> new FingerprintScheme("bad", 0, 3, all))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("profileBucket");

        assertThatThrownBy(() -> new FingerprintScheme("bad", 20, 0, all))
                .isInstanceOf(IllegalArgumentException.class)
                .hasMessageContaining("maxVariants");
    }

    /**
     * ⭐ 方案是值对象，构造之后不该被外部改动。
     *
     * <p>不做防御性拷贝的话，调用方持有的那个 Set 一改，方案就跟着变了 ——
     * 而模拟器会同时持有好几个方案做对照，其中一个被悄悄改掉是最难查的一类 bug。
     */
    @Test
    @DisplayName("构造后修改传入的 Set 不影响方案")
    void fields_areDefensivelyCopied() {
        EnumSet<FingerprintScheme.Field> mutable = EnumSet.allOf(FingerprintScheme.Field.class);
        FingerprintScheme scheme = new FingerprintScheme("s", 20, 3, mutable);

        mutable.remove(FingerprintScheme.Field.BUDGET);

        assertThat(scheme.fields()).contains(FingerprintScheme.Field.BUDGET);
        assertThat(scheme.compute(FingerprintInput.from(sample()))).contains("b=30");
    }

    // ── 测试数据 ──

    private static IntentRequest sample() {
        return sampleWithBuildConcentration(87);
    }

    private static IntentRequest sampleWithBuildConcentration(double build) {
        return new IntentRequest(
                1,
                "run-1",
                1,
                3,
                30,
                Map.of(
                        "buildConcentration", build,
                        "combatEfficiency", 40.0,
                        "strategySwitch", 0.0,
                        "survivalPressure", 0.0,
                        "resourceSurplus", 50.0,
                        "confidence", 0.9,
                        "dominantArchetype", "Archetype.Ranger"),
                List.of(new IntentRequest.AvailableRule("Rule.Ammo", "light", 10, List.of())),
                List.of("Enemy.Grunt"),
                List.of());
    }
}
