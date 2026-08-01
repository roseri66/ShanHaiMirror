package com.shanhai.director.api;

import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

/**
 * 决策端点。
 *
 * <p><b>M0 阶段这是一个 stub</b>：返回固定 Intent，不调 LLM。
 * 存在的意义是把整条链路先打通——客户端能发出合规的上行请求、
 * 服务端能解析、响应能过客户端的四道护栏并落进报告卡。
 * 真实 LLM 调用是 M1 的事。
 *
 * <p><b>护栏不在这里</b>，也永远不会搬过来（D-23 的核心否决）。
 * 服务端返回的 Intent 与其它 Provider 的输出一样，要过客户端四道护栏，
 * 不因为"是自己的服务"就有免检特权。
 */
@RestController
public class IntentController {

    private static final Logger log = LoggerFactory.getLogger(IntentController.class);

    /** 上行路径。<b>必须与 UE 侧 FSHMRemoteProvider::IntentPath 一致</b>，那边有测试钉着。 */
    public static final String INTENT_PATH = "/v1/director/intent";

    /** 本服务能处理的上行契约版本。客户端发来更高的版本就明确拒绝，不猜。 */
    private static final int SUPPORTED_SCHEMA_VERSION = 1;

    @PostMapping(INTENT_PATH)
    public ResponseEntity<DirectorIntent> intent(@RequestBody(required = false) IntentRequest request) {
        final long startedAt = System.nanoTime();

        // --- 版本不符：明确拒绝，不静默错读 ---
        // 字段同名而语义已变时，返回一个"看起来对"的错误结论
        // 比返回 400 糟糕得多。这正是当初把 schemaVersion 钉在顶层第一个
        // 字段换来的红利。客户端收到非 200 会照常降级本地，玩家零感知。
        if (request == null || request.schemaVersion() == null) {
            log.warn("请求缺少 schemaVersion，拒绝");
            return ResponseEntity.badRequest().build();
        }
        if (request.schemaVersion() != SUPPORTED_SCHEMA_VERSION) {
            log.warn("上行契约版本不符：收到 {}，本服务支持 {} —— 拒绝，客户端将降级本地",
                    request.schemaVersion(), SUPPORTED_SCHEMA_VERSION);
            return ResponseEntity.badRequest().build();
        }

        log.info("收到决策请求：runId={} floor={}/{} budget={} 候选规则={} 候选原型={} 历史={}层",
                request.runId(), request.floorIndex(), request.totalFloors(),
                request.challengeBudget(),
                request.availableRules() == null ? 0 : request.availableRules().size(),
                request.availableArchetypes() == null ? 0 : request.availableArchetypes().size(),
                request.decisionHistory() == null ? 0 : request.decisionHistory().size());

        final DirectorIntent intent = buildStubIntent();
        final long elapsedMs = (System.nanoTime() - startedAt) / 1_000_000L;

        return ResponseEntity.ok()
                // ServerLocal = 服务端自己产的，不是 LLM 产的。
                // **M1 接上真实 LLM 之前，这里绝不能报 Llm** ——
                // 客户端会把它记进决策日志的 trace，谎报来源等于往证据链里掺假。
                .header("X-SHM-Source", "ServerLocal")
                .header("X-SHM-Cache", "miss")
                .header("X-SHM-Elapsed-Ms", String.valueOf(elapsedMs))
                .body(intent);
    }

    /**
     * M0 的固定 Intent。
     *
     * <p>三个刻意的选择：
     * <ul>
     *   <li><b>配比明显偏 Tank</b>（0.40）——本地 Provider 不会产出这个分布，
     *       所以在游戏里一眼能认出"这一层的配比确实来自 stub"，
     *       这正是 M0 的端到端验收判据</li>
     *   <li><b>只选一条 light 规则</b>（cost 10）——F1 预算 30、F2 预算 55，
     *       两层都买得起，不会被 Budget 护栏拦下</li>
     *   <li><b>选 Rule.Cooldown</b>——它在 RuleTable.csv 里 ConflictsWith 为空，
     *       完全绕开 Conflict 护栏。stub 的目的是验证链路通不通，
     *       不是去触发护栏（那是 M2 的降级回归要做的事）</li>
     * </ul>
     *
     * <p>台词直说自己是 stub。**不伪装成 LLM 生成的内容**：
     * 万一它出现在截图或演示里，也能自证来历，不会让人误以为是模型说的话。
     */
    private DirectorIntent buildStubIntent() {
        // LinkedHashMap 保序，日志和抓包里看着稳定
        final Map<String, Double> weights = new LinkedHashMap<>();
        weights.put("Enemy.Tank", 0.40);
        weights.put("Enemy.Rush", 0.30);
        weights.put("Enemy.Grunt", 0.20);
        weights.put("Enemy.Shooter", 0.10);   // 和 = 1.00，Schema 护栏要求

        return new DirectorIntent(
                "Pressure",
                weights,
                List.of(new DirectorIntent.RuleIntent("Rule.Cooldown", "light")),
                "【DirectorService stub】这句话来自服务端占位实现，不是 LLM 生成的。",
                "M0 固定响应：验证链路连通，不代表任何真实决策逻辑。"
        );
    }
}
