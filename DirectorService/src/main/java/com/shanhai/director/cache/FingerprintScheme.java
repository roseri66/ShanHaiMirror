package com.shanhai.director.cache;

import java.util.EnumSet;
import java.util.Objects;
import java.util.Set;

/**
 * 一套「指纹方案」：把哪些字段纳入指纹、连续量按多宽分桶、同一指纹保留几条候选。
 *
 * <h2>它为什么存在</h2>
 *
 * M3 定下的两个参数（桶宽 20 分、候选 3 条）当时是<b>拍的</b>，代码注释里也写明了
 * 「待回流数据校准」。M5（决策 D-24）要做的就是校准它们 —— 而校准的前提是能回答
 * <b>「换一套方案会怎样」</b>，所以方案本身必须是一个可以被构造出多个实例的值对象，
 * 而不是散落在代码里的几个常量。
 *
 * <h2>⭐ 唯一的算法实现</h2>
 *
 * 线上查缓存和离线跑模拟，都走 {@link #compute(FingerprintInput)}。
 * <b>不允许出现第二份指纹算法</b> —— 两份必然漂移，而一旦漂移，
 * 模拟出来的命中率就和真实的不可比，这套东西也就失去了全部意义。
 * （这与本项目否决「护栏 C++/Java 双写」是同一条判据。）
 *
 * <h2>读代码时发现的、待数据验证的两个假设</h2>
 *
 * <ol>
 *   <li><b>{@link Field#BUDGET} 很可能是冗余的。</b> 客户端的
 *       {@code ChallengeBudgetForFloor} 是 {@code FloorIndex} 的纯函数
 *       （{@code FloorIndex <= 0 ? 0 : 5 + FloorIndex * 25}），
 *       所以 budget 不携带 floor 之外的任何信息。
 *       <b>但服务端收到的是客户端传来的值，不该假设它一定是那个公式</b> ——
 *       所以这是一个<b>假设</b>，由 M5 的数据来验证（去掉它指纹数变不变）。</li>
 *   <li><b>{@link Field#FLOOR} 把一局的两次请求强制切成两条指纹。</b>
 *       一局只发 2 次决策请求（F0 是观察层不走 Provider），而 F1/F2 的 floor 不同 ——
 *       于是同一条指纹要攒满候选必须跨局，而画像还会随局数漂移。
 *       {@link IntentCache} 的注释里记着「用户实测打了 3 把，一次都没命中」。</li>
 * </ol>
 *
 * <p><b>⚠️ 在 M5 的数据出来之前，不要凭这两条假设改动 {@link #CURRENT}。</b>
 * 这个类的存在就是为了让它们可以被验证，而不是为了让它们被直接采信。
 *
 * @param name           方案名，仅用于报告可读
 * @param profileBucket  五维画像的分桶宽度（分）
 * @param maxVariants    同一指纹保留几条候选
 * @param fields         哪些字段纳入指纹
 * @since M5
 */
public record FingerprintScheme(
        String name,
        int profileBucket,
        int maxVariants,
        Set<Field> fields) {

    /** 指纹可以纳入的字段。给模拟器用来做「去掉某个字段会怎样」的对照。 */
    public enum Field {
        /** 层号。 */
        FLOOR,
        /** 挑战预算。⚠️ 疑似被 FLOOR 完全决定，见类注释。 */
        BUDGET,
        /** 四维画像（分桶后）。 */
        PROFILE,
        /** 置信度（分档后）。 */
        CONFIDENCE,
        /** 主导原型。 */
        ARCHETYPE,
        /** 候选规则集合。 */
        RULES,
        /** 候选敌人原型集合。 */
        AVAIL_ARCHETYPES,
        /** 历史决策的规则 tag 集合。 */
        HISTORY
    }

    /**
     * 画像分桶宽度。<b>拍的值</b>，待 M5 的回流数据校准。
     *
     * <p>取 20 的理由是「87 分和 85 分不该算两种玩家」，但 20 这个具体数字没有依据。
     */
    public static final int DEFAULT_PROFILE_BUCKET = 20;

    /**
     * 同一指纹保留几条候选。<b>拍的值</b>，待 M5 的回流数据校准。
     *
     * <p>它存在的理由是明确的：缓存整个 Intent 会让台词重复，而台词恰恰是最该每次不一样的字段。
     * 保留多条、命中时随机取一条，能在省钱和「同类玩家听到不同说法」之间取得平衡。
     * <b>但「3」这个具体数字同样没有依据。</b>
     */
    public static final int DEFAULT_MAX_VARIANTS = 3;

    /** 线上正在用的方案。<b>模拟器的正确性锚点</b>：用它模拟出来的结果必须等于数据库里的真实统计。 */
    public static final FingerprintScheme CURRENT = new FingerprintScheme(
            "current", DEFAULT_PROFILE_BUCKET, DEFAULT_MAX_VARIANTS, EnumSet.allOf(Field.class));

    public FingerprintScheme {
        Objects.requireNonNull(name, "name");
        Objects.requireNonNull(fields, "fields");
        if (profileBucket <= 0) {
            throw new IllegalArgumentException("profileBucket 必须为正，实得 " + profileBucket);
        }
        if (maxVariants <= 0) {
            throw new IllegalArgumentException("maxVariants 必须为正，实得 " + maxVariants);
        }
        if (fields.isEmpty()) {
            // 空字段集会让所有请求算出同一条指纹（空串），命中率恒为 100% ——
            // 那不是一个"更好的方案"，是一个退化成"永远返回上一次结果"的 bug。
            // 让它构造期就失败，而不是在报告里给出一个诱人的假数字。
            throw new IllegalArgumentException("fields 不能为空：空指纹会让所有请求命中同一条缓存");
        }
        // 防御性拷贝：方案是值对象，构造之后不该被外部改动
        fields = EnumSet.copyOf(fields);
    }

    /**
     * 算指纹。
     *
     * <p><b>输出格式与 M3 的原实现逐字节一致</b>（在 {@link #CURRENT} 下），
     * 这样重构不会让既有缓存语义发生任何变化 —— 既有的 11 条缓存测试是这条的守卫。
     */
    public String compute(FingerprintInput in) {
        StringBuilder sb = new StringBuilder();

        if (fields.contains(Field.FLOOR)) {
            seg(sb, "f", in.floorIndex());
        }
        if (fields.contains(Field.BUDGET)) {
            seg(sb, "b", in.challengeBudget());
        }
        if (fields.contains(Field.PROFILE)) {
            // 键名取维度名前两个字母，与 M3 原实现一致：bu / co / st / su。
            // ⚠️ resourceSurplus 不在此列 —— 它恒为 50（道具系统被 D-09 砍了，无数据源），
            //    入 key 只是给每条指纹加一个常量后缀，纯浪费。
            seg(sb, "bu", bucket(in.buildConcentration()));
            seg(sb, "co", bucket(in.combatEfficiency()));
            seg(sb, "st", bucket(in.strategySwitch()));
            seg(sb, "su", bucket(in.survivalPressure()));
        }
        if (fields.contains(Field.CONFIDENCE)) {
            seg(sb, "c", confidenceTier(in.confidence()));
        }
        if (fields.contains(Field.ARCHETYPE)) {
            seg(sb, "a", in.dominantArchetype());
        }
        if (fields.contains(Field.RULES)) {
            seg(sb, "r", in.availableRulesKey());
        }
        if (fields.contains(Field.AVAIL_ARCHETYPES)) {
            seg(sb, "e", in.availableArchKey());
        }
        if (fields.contains(Field.HISTORY)) {
            seg(sb, "h", in.historyTagsKey());
        }

        return sb.toString();
    }

    /** 去掉某个字段之后的方案。模拟器用它做对照，例如验证 BUDGET 是不是冗余的。 */
    public FingerprintScheme without(Field field) {
        EnumSet<Field> remaining = EnumSet.copyOf(fields);
        remaining.remove(field);
        return new FingerprintScheme(name + "-no" + field.name().toLowerCase(),
                profileBucket, maxVariants, remaining);
    }

    /** 换一个桶宽的方案。 */
    public FingerprintScheme withBucket(int newBucket) {
        return new FingerprintScheme(name + "-bucket" + newBucket, newBucket, maxVariants, fields);
    }

    /** 换一个候选数的方案。 */
    public FingerprintScheme withMaxVariants(int newMax) {
        return new FingerprintScheme(name + "-var" + newMax, profileBucket, newMax, fields);
    }

    private static void seg(StringBuilder sb, String key, Object value) {
        // 首段不带分隔符 —— 这样 CURRENT 的输出与 M3 原实现逐字节一致
        if (sb.length() > 0) {
            sb.append('|');
        }
        sb.append(key).append('=').append(value);
    }

    /**
     * 画像分桶。把 {@code [0, 100]} 切成等宽的若干桶。
     *
     * <h2>⚠️ 这里修过一个缺陷：满分 100 曾经自成一桶</h2>
     *
     * 原实现是 {@code (int)(v / width)}。问题在于 <b>100 是闭区间的上界</b>，
     * 而 {@code 100 / width} 总是比 {@code 99.9 / width} 多一档 ——
     * 于是<b>「满分」永远单独占一个只装它自己的桶</b>。
     *
     * <p>桶宽 20 时：{@code 96.8 → 4}，而 {@code 100 → 5}。
     * 两个只差 3.2 分的画像被当成了两种玩家 —— 这恰恰违背了分桶的初衷
     * （「87 分和 85 分不该算两种玩家」）。
     *
     * <p><b>它的实际影响远超预期</b>：M5-5 的 32 条真实流水里，
     * {@code buildConcentration} 等于 100 的有 <b>26 条（81%）</b> ——
     * 因为「专精单一 Build」的玩家算出来就是满分。<b>这个缺陷几乎每次都命中。</b>
     *
     * <h2>它是怎么被发现的</h2>
     *
     * 不是看代码看出来的，是<b>顺着一个反常现象查出来的</b>：
     * 模拟显示<b>桶宽 34 的命中率反而比桶宽 50 和 100 高</b>。
     * 「桶越宽合并越多」是常识，反过来就说明分桶本身有问题 ——
     * 桶宽 34 恰好让 100 和 86~97 落进同一桶，而 50 / 100 又把它分了出去。
     *
     * <p><b>一个不符合直觉的数据点，比一百个符合直觉的更值得追。</b>
     */
    private int bucket(double v) {
        double clamped = Math.max(0, Math.min(100, v));
        int raw = (int) (clamped / profileBucket);
        // 最后一桶是闭区间：把 100 并进 [maxBucket*width, 100]，而不是让它自成一桶
        int maxBucket = (100 - 1) / profileBucket;
        return Math.min(raw, maxBucket);
    }

    /**
     * 置信度分三档。
     *
     * <p>刻意<b>不</b>分更细：四道护栏里只有 Fairness 用到置信度，而它只在 0.6 这一个阈值上
     * 有行为差异。分得比消费方的精度更细就是<b>假精度</b> —— 它只会降低命中率，不会让结果更准。
     */
    private static String confidenceTier(double c) {
        if (c < 0.6) {
            return "lo";
        }
        return c <= 0.8 ? "mid" : "hi";
    }
}
