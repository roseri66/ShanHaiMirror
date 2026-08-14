package com.shanhai.director.persistence;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

import com.shanhai.director.api.IntentRequest;
import com.shanhai.director.cache.CacheOutcome;
import com.shanhai.director.cache.FingerprintInput;

/**
 * 一次决策请求的流水记录，对应 {@code intent_request} 表的一行（M5，决策 D-24）。
 *
 * <h2>⭐ 为什么内嵌 {@link FingerprintInput} 而不是把字段摊平</h2>
 *
 * 因为 {@code FingerprintInput} 正好就是「参与指纹计算的那些字段」的集合 ——
 * 而本表拆列存储的理由就是要让这些字段可以被重新分桶、重新算指纹。
 *
 * <p>内嵌带来一个直接的好处：<b>从数据库读出一行，就能直接喂给
 * {@link com.shanhai.director.cache.FingerprintScheme#compute} 跑模拟</b>，
 * 中间不需要任何转换代码。少一层转换就少一处漂移的机会。
 *
 * <h2>口径</h2>
 *
 * ⚠️ <b>本记录只覆盖「到达服务端的请求」。</b>客户端降级到本地 Provider 的那些决策，
 * 服务端根本不知道，不会出现在这里。所以它能回答「缓存效果怎么样」，
 * <b>回答不了「整体降级率是多少」</b>。后者需要客户端上报，D-24 明确本次不做。
 *
 * @param runId            一局的标识。刻意不入指纹——入了必然永不命中
 * @param schemaVersion    上行契约版本，日后契约演进时靠它区分口径
 * @param input            参与指纹计算的原始字段（含 floorIndex / challengeBudget）
 * @param fingerprint      当时那套方案算出来的指纹。**模拟器的正确性锚点**
 * @param cacheOutcome     命中 / 从没见过 / 预热期补充
 * @param variantCount     查询发生时该指纹下已有几条候选
 * @param source           这次的 Intent 最终来自哪里：Cache / Llm / ServerLocal
 * @param httpStatus       返回给客户端的状态码
 * @param latencyMs        服务端自报耗时，不含网络往返
 * @param profileExtraJson 不参与指纹但值得留档的画像字段（如恒为 50 的 resourceSurplus）
 * @param debugFlags       ⭐ D-25：调试作弊倍率，如 {@code "dmg=4.00,hp=0.40"}；
 *                         {@code null} 表示真实游玩。<b>这批数据能答结构问题，不能答比率问题</b>
 * @param createdAt        记录时间
 * @since M5
 */
public record IntentRecord(
        String runId,
        int schemaVersion,
        FingerprintInput input,
        String fingerprint,
        CacheOutcome cacheOutcome,
        int variantCount,
        String source,
        int httpStatus,
        int latencyMs,
        String profileExtraJson,
        String debugFlags,
        Instant createdAt) {

    public IntentRecord {
        Objects.requireNonNull(input, "input");
        Objects.requireNonNull(fingerprint, "fingerprint");
        Objects.requireNonNull(cacheOutcome, "cacheOutcome");
        Objects.requireNonNull(createdAt, "createdAt");
        // runId 允许为空串但不允许为 null —— 表里是 NOT NULL。
        // 客户端理论上可能不传 runId（上行契约里它不是必填），
        // 那种情况记成空串比让整条记录丢掉好：**样本本来就少，不该因为一个可选字段而丢样本。**
        runId = runId == null ? "" : runId;
        source = source == null ? "unknown" : source;
        // 空串与 null 在这里必须归一：分析时要按「debug_flags IS NULL = 真实数据」来切，
        // 混进空串会让那个判据悄悄漏掉一部分行。
        debugFlags = (debugFlags == null || debugFlags.isBlank()) ? null : debugFlags.strip();
    }

    /** 画像里参与指纹计算的字段。留档用的 extra 就是「画像的全部字段减去这些」。 */
    private static final java.util.Set<String> FINGERPRINT_PROFILE_KEYS = java.util.Set.of(
            "buildConcentration", "combatEfficiency", "strategySwitch",
            "survivalPressure", "confidence", "dominantArchetype");

    /**
     * 从一次真实请求与它的处理结果构造记录。
     *
     * @param fingerprint 由 {@link com.shanhai.director.cache.IntentCache.LookupResult} 带出来，
     *                    **不在这里重算** —— 重算一次不但浪费，还留下了两处算法漂移的机会
     */
    public static IntentRecord of(IntentRequest request,
                                  String fingerprint,
                                  CacheOutcome outcome,
                                  int variantCount,
                                  String source,
                                  int httpStatus,
                                  long latencyMs,
                                  String debugFlags,
                                  Instant now) {
        return new IntentRecord(
                request.runId(),
                request.schemaVersion() == null ? 0 : request.schemaVersion(),
                FingerprintInput.from(request),
                fingerprint,
                outcome,
                variantCount,
                source,
                httpStatus,
                (int) Math.min(Integer.MAX_VALUE, Math.max(0, latencyMs)),
                extraProfileJson(request.profile()),
                debugFlags,
                now);
    }

    /**
     * 把不参与指纹的画像字段序列化成一小段 JSON。
     *
     * <p>手拼而不引 Jackson 的 ObjectMapper，理由是这里的值域已知且极窄
     * （数字与短字符串），而这段代码在写入路径上、每条记录都要跑一次。
     * <b>但正因为是手拼，字符串值必须转义</b> —— 见 {@link #escape}。
     */
    static String extraProfileJson(Map<String, Object> profile) {
        if (profile == null || profile.isEmpty()) {
            return null;
        }
        Map<String, Object> extra = new LinkedHashMap<>();
        for (Map.Entry<String, Object> e : profile.entrySet()) {
            if (!FINGERPRINT_PROFILE_KEYS.contains(e.getKey())) {
                extra.put(e.getKey(), e.getValue());
            }
        }
        if (extra.isEmpty()) {
            return null;
        }
        StringBuilder sb = new StringBuilder("{");
        boolean first = true;
        for (Map.Entry<String, Object> e : extra.entrySet()) {
            if (!first) {
                sb.append(',');
            }
            first = false;
            sb.append('"').append(escape(e.getKey())).append("\":");
            Object v = e.getValue();
            if (v instanceof Number || v instanceof Boolean) {
                sb.append(v);
            } else if (v == null) {
                sb.append("null");
            } else {
                sb.append('"').append(escape(v.toString())).append('"');
            }
        }
        return sb.append('}').toString();
    }

    /**
     * JSON 字符串转义。
     *
     * <p>⚠️ 少了它，一个含引号或反斜杠的画像字段就能让整列变成非法 JSON ——
     * 而 MySQL 的 JSON 列会在插入时直接报错，**于是一条坏数据会让整批落库失败**。
     * 值域窄不是不转义的理由，只是不引 Jackson 的理由。
     */
    private static String escape(String s) {
        StringBuilder sb = new StringBuilder(s.length() + 8);
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            switch (c) {
                case '"' -> sb.append("\\\"");
                case '\\' -> sb.append("\\\\");
                case '\n' -> sb.append("\\n");
                case '\r' -> sb.append("\\r");
                case '\t' -> sb.append("\\t");
                default -> {
                    if (c < 0x20) {
                        sb.append(String.format("\\u%04x", (int) c));
                    } else {
                        sb.append(c);
                    }
                }
            }
        }
        return sb.toString();
    }
}
