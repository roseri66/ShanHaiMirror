package com.shanhai.director.cache;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import com.shanhai.director.api.IntentRequest;

/**
 * 算指纹所需要的、且**只需要**的那些输入。
 *
 * <h2>它为什么存在</h2>
 *
 * 指纹有两个来源：
 * <ul>
 *   <li><b>线上</b>：从 {@link IntentRequest} 算，用于缓存查找</li>
 *   <li><b>离线</b>：从数据库里的历史流水算，用于回答「换一套指纹方案会怎样」（M5，决策 D-24）</li>
 * </ul>
 *
 * 两条路径必须用**同一份**算法。若各写一份，两边对「集合要不要排序」「置信度怎么分档」
 * 只要有一点分歧，模拟出来的命中率就和真实的不可比 —— 而那正是这套模拟器唯一的用处。
 * <b>这与本项目否决「护栏双写」是同一条判据：逻辑双写必然漂移。</b>
 *
 * <p>所以把「算指纹的输入」提成这个类型，两条路径都先转成它，再交给
 * {@link FingerprintScheme#compute(FingerprintInput)}。
 *
 * <h2>三个 key 字段为什么是已经排好序的字符串</h2>
 *
 * {@code availableRulesKey} / {@code availableArchKey} / {@code historyTagsKey} 是
 * <b>排序后拼接的结果</b>，不是原始列表。原因是数据库里存的就是这个形态 ——
 * 排序发生在写入时，读出来直接可用。让两条路径在这里对齐，比让离线路径重新排一遍更不容易错。
 *
 * @since M5
 */
public record FingerprintInput(
        int floorIndex,
        int challengeBudget,
        double buildConcentration,
        double combatEfficiency,
        double strategySwitch,
        double survivalPressure,
        double confidence,
        String dominantArchetype,
        String availableRulesKey,
        String availableArchKey,
        String historyTagsKey) {

    /** 线上路径：从上行请求提取。 */
    public static FingerprintInput from(IntentRequest req) {
        Map<String, Object> profile = req.profile();
        return new FingerprintInput(
                orZero(req.floorIndex()),
                orZero(req.challengeBudget()),
                num(profile, "buildConcentration"),
                num(profile, "combatEfficiency"),
                num(profile, "strategySwitch"),
                num(profile, "survivalPressure"),
                num(profile, "confidence"),
                str(profile, "dominantArchetype"),
                sortedJoin(ruleKeys(req)),
                sortedJoin(req.availableArchetypes()),
                sortedJoin(historyTags(req)));
    }

    // ── 以下为提取与规范化的工具方法。它们从 IntentCache 搬过来，行为一字未改 ──

    /**
     * 集合语义：排序后拼接。
     *
     * <p>不排序的话，同一组候选换个顺序就是另一条指纹，命中率会莫名其妙地低，而且极难发现原因。
     */
    public static String sortedJoin(List<String> items) {
        if (items == null || items.isEmpty()) {
            return "-";
        }
        List<String> copy = new ArrayList<>(items);
        copy.sort(String::compareTo);
        return String.join(",", copy);
    }

    private static List<String> ruleKeys(IntentRequest req) {
        List<String> out = new ArrayList<>();
        if (req.availableRules() != null) {
            for (IntentRequest.AvailableRule r : req.availableRules()) {
                out.add(r.tag() + ":" + r.level());
            }
        }
        return out;
    }

    /**
     * 历史只取规则 tag。
     *
     * <p>Fairness 护栏只关心「这条规则用过没有」。带上层号的话每层都是新指纹，等于缓存失效。
     */
    private static List<String> historyTags(IntentRequest req) {
        List<String> out = new ArrayList<>();
        if (req.decisionHistory() != null) {
            for (IntentRequest.HistoryEntry e : req.decisionHistory()) {
                if (e.ruleTags() != null) {
                    out.addAll(e.ruleTags());
                }
            }
        }
        return out;
    }

    private static int orZero(Integer v) {
        return v == null ? 0 : v;
    }

    private static double num(Map<String, Object> m, String key) {
        if (m == null) {
            return 0.0;
        }
        Object v = m.get(key);
        return v instanceof Number n ? n.doubleValue() : 0.0;
    }

    private static String str(Map<String, Object> m, String key) {
        if (m == null) {
            return "-";
        }
        Object v = m.get(key);
        return v instanceof String s && !s.isBlank() ? s : "-";
    }
}
