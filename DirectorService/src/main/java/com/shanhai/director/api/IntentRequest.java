package com.shanhai.director.api;

import java.util.List;
import java.util.Map;

/**
 * 上行请求体。
 *
 * <p><b>契约真源在 UE 侧</b>：{@code Director/SHMDirectorWireFormat.h}。
 * 这个 record 是它的 Java 镜像，改 .h 必须同步改这里——
 * 这是本项目第二处跨语言契约耦合（第一处是 WebReplay 的 TS 镜像），
 * 是选 monorepo 的理由：两处都在同一个仓库里，至少能靠 grep 找全。
 *
 * <p>字段全部可空：不信任客户端输入是基本功。M0 阶段只用到 schemaVersion
 * 与 floorIndex，其余字段先接住不用——但**必须先声明**，否则 Jackson 默认
 * 会因未知字段报错，而"客户端多发了个字段"不该让整个请求 400。
 *
 * <p>注意 decisionHistory 的条目只有 floorIndex 与 ruleTags 两个字段。
 * 设计文档 §5.1 的示例里还画了 challengeLevel 与 playerAdapted，
 * 但 UE 侧的 FDirectorHistoryEntry 没有这两项数据，也没有任何地方计算
 * playerAdapted——凭空发过来只能是常量，那是把产品算不出来的状态当成记录。
 */
public record IntentRequest(
        Integer schemaVersion,
        String runId,
        Integer floorIndex,
        Integer totalFloors,
        Integer challengeBudget,
        Map<String, Object> profile,
        List<AvailableRule> availableRules,
        List<String> availableArchetypes,
        List<HistoryEntry> decisionHistory
) {
    /**
     * 候选规则：客户端已经把 Cost 与互斥信息拷进来了，服务端不需要再查表。
     *
     * <p>{@code conflictsWith} 必须注入进 prompt。2026-07-28 实测——不给它
     * LLM 只能盲选，DeepSeek 同时挑了「弹药↓ + 远程伤害↓」，对远程玩家是
     * 无解组合，被客户端 Conflict 护栏拒并白白降级一次。
     * <b>候选集要给全，才叫"只给安全选项"。</b>
     *
     * <p>注意这不等于让服务端做互斥判定——判定仍在客户端护栏（D-23 的核心否决）。
     * 发过来只是让 LLM 别去选注定被拒的组合。
     */
    public record AvailableRule(String tag, String level, Integer cost, List<String> conflictsWith) {
    }

    /** 往层决策：Fairness 只关心"这条规则用过没有"，故只有 tag 列表。 */
    public record HistoryEntry(Integer floorIndex, List<String> ruleTags) {
    }
}
