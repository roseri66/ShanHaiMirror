package com.shanhai.director.llm;

import java.util.List;
import java.util.Map;

import org.springframework.stereotype.Component;

import com.shanhai.director.api.IntentRequest;

/**
 * User prompt 组装。
 *
 * <p><b>这是 SHMPromptBuilder.cpp::BuildUserPrompt 的逐行搬迁</b>
 * （2026-07-31，D-23 / M1）。搬迁时没有做任何"顺手改进"——
 * 那个版本是唯一被真实 LLM 验证过的，改写要另起一次，
 * 出问题才分得清是搬迁的锅还是改写的锅。
 *
 * <p>几处必须与 C++ 端一致、错了不报错只是 prompt 变味的地方：
 * <ul>
 *   <li><b>层号显示 +1</b>：C++ 写的是 {@code FloorIndex + 1}，即内部 0-based、
 *       给 LLM 看 1-based。历史条目同理。少加这个 1，LLM 看到的是"第 0 层"</li>
 *   <li><b>画像只列五项</b>，不含 resourceSurplus——它恒为 50（D-09 砍了道具系统，
 *       无数据源），列进去等于告诉 LLM 一个假观测</li>
 *   <li><b>互斥信息必须注入</b>：不给它 LLM 只能盲选。2026-07-28 实测 DeepSeek
 *       同时挑了「弹药↓ + 远程伤害↓」，对远程玩家是无解组合，被 Conflict 护栏拒。
 *       候选集要给全，才叫"只给安全选项"</li>
 * </ul>
 */
@Component
public class PromptBuilder {

    /** 与 C++ 端一致：内部 0-based，展示给 LLM 时 1-based。 */
    private static final int FLOOR_DISPLAY_OFFSET = 1;

    public String buildUserPrompt(IntentRequest req) {
        StringBuilder out = new StringBuilder();

        out.append(String.format("【当前进度】第 %d 层 / 共 %d 层%n",
                orZero(req.floorIndex()) + FLOOR_DISPLAY_OFFSET, orZero(req.totalFloors())));

        appendProfile(out, req.profile());

        out.append(String.format("%n【挑战预算】%d（所选规则 cost 之和不得超过它）%n",
                orZero(req.challengeBudget())));

        appendArchetypes(out, req.availableArchetypes());
        appendRules(out, req.availableRules());
        appendHistory(out, req.decisionHistory());

        out.append(String.format("%n请给出这一层的导演决策 JSON。"));
        return out.toString();
    }

    private void appendProfile(StringBuilder out, Map<String, Object> profile) {
        out.append(String.format("%n【玩家画像】（0-100）%n"));
        if (profile == null) {
            return;
        }
        // 五项，顺序与 C++ 端一致。**不含 resourceSurplus**——它恒为 50，
        // 无数据源（D-09），列进去是把常量伪装成观测结论。
        out.append(String.format("  Build 集中度 : %.0f（越高越依赖单一打法）%n", num(profile, "buildConcentration")));
        out.append(String.format("  战斗效率     : %.0f%n", num(profile, "combatEfficiency")));
        out.append(String.format("  策略切换意愿 : %.0f（越低越像「一招鲜」）%n", num(profile, "strategySwitch")));
        out.append(String.format("  生存压力     : %.0f（越高说明玩家越吃力）%n", num(profile, "survivalPressure")));
        out.append(String.format("  判断置信度   : %.2f（低于 0.6 时不要激进针对）%n", num(profile, "confidence")));

        Object archetype = profile.get("dominantArchetype");
        if (archetype instanceof String s && !s.isBlank()) {
            out.append(String.format("  主导原型     : %s%n", s));
        }

        // 主力打法：客户端在 D-23/M1 时补进上行契约的字段。
        // 没有它这一行就不会出现，服务端产出会与直连模式不一致。
        Object tags = profile.get("primaryBuildTags");
        if (tags instanceof List<?> list && !list.isEmpty()) {
            out.append("  主力打法     :");
            for (Object t : list) {
                out.append(' ').append(t);
            }
            out.append(System.lineSeparator());
        }
    }

    private void appendArchetypes(StringBuilder out, List<String> archetypes) {
        out.append(String.format("%n【可用敌人原型】（enemyWeights 的键只能从这里选）%n"));
        if (archetypes == null) {
            return;
        }
        for (String tag : archetypes) {
            out.append(String.format("  %s%n", tag));
        }
    }

    private void appendRules(StringBuilder out, List<IntentRequest.AvailableRule> rules) {
        out.append(String.format("%n【可用规则】（ruleIntents 只能从这些 (tag, level) 组合里选）%n"));
        if (rules == null || rules.isEmpty()) {
            out.append(String.format("  （本层无可用规则，ruleIntents 请返回空数组）%n"));
            return;
        }
        for (IntentRequest.AvailableRule rule : rules) {
            out.append(String.format("  tag=%s level=%s cost=%d",
                    rule.tag(), rule.level(), orZero(rule.cost())));

            // 互斥信息必须一并注入 —— 不给它 LLM 只能盲选。
            // 措辞与 C++ 端逐字一致：改这里会让服务端产出与直连模式不一致。
            if (rule.conflictsWith() != null && !rule.conflictsWith().isEmpty()) {
                out.append("  【不可与以下规则同时选用：");
                for (String c : rule.conflictsWith()) {
                    out.append(' ').append(c);
                }
                out.append('】');
            }
            out.append(System.lineSeparator());
        }
    }

    private void appendHistory(StringBuilder out, List<IntentRequest.HistoryEntry> history) {
        if (history == null || history.isEmpty()) {
            return;
        }
        out.append(String.format("%n【最近几层已用过的规则】（避免连续重复针对）%n"));
        for (IntentRequest.HistoryEntry entry : history) {
            out.append(String.format("  第 %d 层:", orZero(entry.floorIndex()) + FLOOR_DISPLAY_OFFSET));
            if (entry.ruleTags() == null || entry.ruleTags().isEmpty()) {
                out.append(" （无）");
            } else {
                for (String tag : entry.ruleTags()) {
                    out.append(' ').append(tag);
                }
            }
            out.append(System.lineSeparator());
        }
    }

    private static int orZero(Integer v) {
        return v == null ? 0 : v;
    }

    private static double num(Map<String, Object> m, String key) {
        Object v = m.get(key);
        return v instanceof Number n ? n.doubleValue() : 0.0;
    }
}
