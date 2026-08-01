package com.shanhai.director.llm;

import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;
import org.springframework.web.reactive.function.client.WebClient;
import org.springframework.web.reactive.function.client.WebClientResponseException;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.shanhai.director.api.DirectorIntent;
import com.shanhai.director.api.IntentRequest;

import reactor.core.publisher.Mono;

/**
 * LLM 上游客户端（OpenAI 兼容 chat/completions）。
 *
 * <p>用 WebClient 而非 RestTemplate：上游是 4–5 秒的慢调用，
 * 线程不能阻塞在等待上。
 *
 * <p><b>它是可失败的，失败被视为正常路径</b>：超时、非 200、畸形 JSON、
 * 模型拒答，一律返回空 Mono，由 Controller 转成 5xx，客户端照常降级本地。
 * 玩家零感知——这正是"断网完整可玩"的实现方式。
 */
@Component
public class LlmClient {

    private static final Logger log = LoggerFactory.getLogger(LlmClient.class);

    private final WebClient webClient;
    private final LlmProperties llm;
    private final PromptProperties prompt;
    private final PromptBuilder promptBuilder;
    private final ObjectMapper mapper = new ObjectMapper();

    /**
     * 鉴权失败后自我停用。
     *
     * <p>与客户端 FSHMLlmProvider 里那个 {@code bAuthFailed} 同一个道理：
     * <b>key 是错的，重试多少次都是错的</b>。不停用的话每层都白发一次注定 401 的
     * 请求，整局被拖慢却毫无收益（踩坑 #20）。
     *
     * <p>这里用 AtomicBoolean 而非普通 boolean：服务端是多线程并发的，
     * 客户端那个单进程 bool 在这里不够用。这也正是"同一条判据、
     * 两个场合、两个结论"——单机单进程一个 bool 就够，服务端就得考虑并发。
     */
    private final AtomicBoolean authFailed = new AtomicBoolean(false);

    public LlmClient(WebClient.Builder builder, LlmProperties llm,
                     PromptProperties prompt, PromptBuilder promptBuilder) {
        this.llm = llm;
        this.prompt = prompt;
        this.promptBuilder = promptBuilder;
        this.webClient = builder.baseUrl(llm.baseUrlOrDefault()).build();

        // 只报告「有没有」，绝不打印 key 本身
        log.info("LLM 上游初始化：endpoint={} model={} timeout={}s key={}",
                llm.baseUrlOrDefault(), llm.modelOrDefault(),
                llm.timeout().toSeconds(), llm.usable() ? "已配置" : "未配置或不可用");
    }

    /** 是否可用。key 不可用、或已因鉴权失败自我停用时返回 false。 */
    public boolean isAvailable() {
        return llm.usable() && !authFailed.get();
    }

    /**
     * 请求一次决策。
     *
     * @return 成功时是 Intent；任何失败都返回 {@link Mono#empty()}，调用方据此降级
     */
    public Mono<DirectorIntent> requestIntent(IntentRequest request) {
        if (!isAvailable()) {
            log.warn("LLM 不可用（未配置 key 或已因鉴权失败停用），本次直接判失败");
            return Mono.empty();
        }

        final String userPrompt = promptBuilder.buildUserPrompt(request);
        final Map<String, Object> body = Map.of(
                "model", llm.modelOrDefault(),
                "messages", List.of(
                        Map.of("role", "system", "content", prompt.system()),
                        Map.of("role", "user", "content", userPrompt)),
                "temperature", prompt.temperatureOrDefault(),
                // 要求 JSON 输出。不支持此字段的端点会忽略它，
                // 届时靠 system prompt 的约束 + 解析容错兜底
                "response_format", Map.of("type", "json_object"));

        final long startedAt = System.nanoTime();

        return webClient.post()
                .uri("/chat/completions")
                .header("Authorization", "Bearer " + llm.apiKey())
                .header("Content-Type", "application/json")
                .bodyValue(body)
                .retrieve()
                .bodyToMono(String.class)
                .timeout(llm.timeout())
                .mapNotNull(raw -> parseEnvelope(raw, startedAt))
                .onErrorResume(e -> {
                    handleError(e, startedAt);
                    return Mono.empty();
                });
    }

    /** 剥 OpenAI 信封：choices[0].message.content 才是模型输出。 */
    private DirectorIntent parseEnvelope(String raw, long startedAt) {
        final long elapsedMs = (System.nanoTime() - startedAt) / 1_000_000L;
        try {
            JsonNode root = mapper.readTree(raw);
            JsonNode choices = root.path("choices");
            if (!choices.isArray() || choices.isEmpty()) {
                log.warn("上游响应无 choices，耗时 {}ms —— 判失败", elapsedMs);
                return null;
            }
            String content = choices.get(0).path("message").path("content").asText("");
            if (content.isBlank()) {
                log.warn("上游 content 为空（可能是模型拒答），耗时 {}ms —— 判失败", elapsedMs);
                return null;
            }

            // 模型可能仍然套了 markdown 代码围栏，尽管 system prompt 明令禁止。
            // 剥掉它比判失败好——内容本身是对的，没必要为格式浪费一次调用。
            content = stripCodeFence(content);

            DirectorIntent intent = mapper.readValue(content, DirectorIntent.class);
            log.info("上游决策成功，耗时 {}ms，challengeLevel={}", elapsedMs, intent.challengeLevel());
            return intent;
        } catch (Exception e) {
            // 畸形 JSON / 字段类型不符 —— 安全失败，不抛给上层
            log.warn("上游响应解析失败（耗时 {}ms）：{} —— 判失败", elapsedMs, e.getMessage());
            return null;
        }
    }

    private static String stripCodeFence(String s) {
        String t = s.trim();
        if (!t.startsWith("```")) {
            return t;
        }
        int firstNewline = t.indexOf('\n');
        if (firstNewline < 0) {
            return t;
        }
        t = t.substring(firstNewline + 1);
        int fenceEnd = t.lastIndexOf("```");
        return (fenceEnd >= 0 ? t.substring(0, fenceEnd) : t).trim();
    }

    private void handleError(Throwable e, long startedAt) {
        final long elapsedMs = (System.nanoTime() - startedAt) / 1_000_000L;

        if (e instanceof WebClientResponseException http) {
            int code = http.getStatusCode().value();
            if (code == 401 || code == 403) {
                // 只在第一次真正翻转时打 error，避免并发下刷屏
                if (authFailed.compareAndSet(false, true)) {
                    log.error("HTTP {} 鉴权失败 —— API key 无效。**本进程不再尝试调用 LLM**，"
                            + "全部返回失败让客户端降级本地。请检查配置后重启服务。", code);
                }
            } else {
                log.warn("上游 HTTP {}，耗时 {}ms —— 判失败", code, elapsedMs);
            }
            return;
        }
        log.warn("上游调用失败（超时或网络错误），耗时 {}ms：{} —— 判失败",
                elapsedMs, e.getClass().getSimpleName());
    }
}
