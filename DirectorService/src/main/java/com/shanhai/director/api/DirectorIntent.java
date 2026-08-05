package com.shanhai.director.api;

import java.util.List;
import java.util.Map;

/**
 * 下行响应体 = <b>Intent 本体，一字不加</b>。
 *
 * <p>刻意不包信封。meta（来源/耗时/缓存命中）走响应头 X-SHM-*。
 * 这样 body 与 LLM 原始输出、与 {@code Data/ReplayScripts/*.json} 三者
 * <b>字节级同格式</b>，带来三个好处：
 * <ol>
 *   <li>客户端零信封剥离代码，直接复用 {@code FSHMJsonIntent::ParseFromJson}
 *       ——那个"不信任 LLM 输出的第一道关卡"原封不动变成"不信任后端输出"</li>
 *   <li>任何一次真实响应可以直接另存为回放脚本</li>
 *   <li>服务端也能拿现成的回放脚本当单测夹具</li>
 * </ol>
 *
 * <p><b>这里没有任何数值字段，是刻意的</b>（UE 侧 D-15）：
 * ruleIntents 只有 (tag, level)，×0.75 这类倍率由客户端查 DT_Rule 产生。
 * 服务端就算想给数值也给不了——客户端的解析器会把数值字段直接丢弃。
 */
public record DirectorIntent(
        String challengeLevel,
        Map<String, Double> enemyWeights,
        List<RuleIntent> ruleIntents,
        String narration,
        String reason
) {
    /** 规则意图：(标签, 等级)，<b>没有数值</b>。 */
    public record RuleIntent(String tag, String level) {
    }
}
