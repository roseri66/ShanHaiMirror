-- 决策请求流水表（M5，决策 D-24）
--
-- 它记录的是**服务端每处理一次 /v1/director/intent 就留一行**。
--
-- ⚠️ 口径必须说在最前面：这张表只记录「到达服务端的请求」。
--    客户端降级到本地 Provider 的那些决策，服务端根本不知道，不会出现在这里。
--    所以它能回答「缓存效果怎么样」，**回答不了「整体降级率是多少」**。
--    后者需要客户端上报，而 D-24 明确本次不做（会新增第三处跨语言契约耦合）。
--
-- ⭐ 为什么参与指纹计算的字段全部拆成独立列，而不是塞进一个 JSON：
--    因为本表存在的意义是让「换一套指纹方案会怎样」这个问题可以被回答，
--    而模拟器要按不同桶宽重新分桶。拆成列能直接 GROUP BY FLOOR(col / width)；
--    塞进 JSON 就只能把所有行捞进内存再算 —— 几百行当然算得动，
--    但那样这张表就不是「能被查询的数据」，而是一坨备份。
--
-- 用 CREATE TABLE IF NOT EXISTS 是为了可重复执行：Spring 的 sql.init 每次启动都会跑它。

CREATE TABLE IF NOT EXISTS intent_request (
    id                  BIGINT       NOT NULL AUTO_INCREMENT,

    -- ── 请求标识（不入指纹，但用于回溯与分组）──
    run_id              VARCHAR(64)  NOT NULL COMMENT '一局的标识。刻意不入指纹——入了必然永不命中',
    floor_index         INT          NOT NULL,
    schema_version      INT          NOT NULL COMMENT '上行契约版本。日后契约演进时靠它区分口径',

    -- ── ⭐ 参与指纹计算的原始字段：全部拆列 ──
    --    与 IntentCache.fingerprint() 的取值一一对应。改那边必须同步改这里。
    challenge_budget    INT          NOT NULL,
    build_concentration DOUBLE       NOT NULL,
    combat_efficiency   DOUBLE       NOT NULL,
    strategy_switch     DOUBLE       NOT NULL,
    survival_pressure   DOUBLE       NOT NULL,
    confidence          DOUBLE       NOT NULL,
    dominant_archetype  VARCHAR(64)  NOT NULL,
    available_rules_key VARCHAR(512) NOT NULL COMMENT '候选规则 tag/level，排序后拼接（集合语义）',
    available_arch_key  VARCHAR(256) NOT NULL COMMENT '候选敌人原型，排序后拼接',
    history_tags_key    VARCHAR(512) NOT NULL COMMENT '历史决策的规则 tag，排序后拼接（Fairness 只关心用过没有）',

    -- ── 当时实际发生了什么 ──
    -- ⭐ fingerprint 存的是「当时那套参数算出来的值」。
    --    它是模拟器唯一的正确性锚点：用 current 方案模拟出来的结果，
    --    必须等于按本列统计出来的真实结果。不等就是模拟器写错了。
    fingerprint         VARCHAR(512) NOT NULL,
    cache_outcome       VARCHAR(16)  NOT NULL COMMENT 'HIT / MISS_EMPTY / MISS_WARMUP',
    variant_count       INT          NOT NULL COMMENT '该指纹下当时已攒了几条候选',
    source              VARCHAR(16)  NOT NULL COMMENT 'Cache / Llm / ServerLocal',
    http_status         INT          NOT NULL,
    latency_ms          INT          NOT NULL COMMENT '服务端自报耗时，不含网络往返',

    -- ── 不参与指纹但值得留档 ──
    -- resourceSurplus 恒为 50（道具系统被 D-09 砍了，无数据源），
    -- 拆成列等于给每行加一个常量列；但仍然留档 ——
    -- 万一以后有了数据源，历史数据里至少能看出当时它是占位。
    profile_extra_json  JSON         NULL,

    created_at          DATETIME(3)  NOT NULL DEFAULT CURRENT_TIMESTAMP(3),

    PRIMARY KEY (id),

    -- 聚合接口都按时间范围过滤，这是最常用的入口
    KEY idx_created_at (created_at),

    -- ⭐ 等值列（fingerprint）在前、范围列（created_at）在后。
    --    反过来放的话，范围查询之后的列会失效，fingerprint 就用不上了。
    KEY idx_fingerprint_created (fingerprint, created_at),

    -- 按局回溯用。(run_id, floor_index) 也是天然的业务唯一键，
    -- 但刻意不建 UNIQUE：同一局同一层被重复请求（人工用 SHM.DumpDecisionAsync 反复调试）
    -- 是正常现象，那些请求同样是有效样本，不该被去重掉。
    KEY idx_run_floor (run_id, floor_index)

) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
  COMMENT='决策请求流水。用途见 D-24：校准缓存指纹方案，而不是做业务查询';
