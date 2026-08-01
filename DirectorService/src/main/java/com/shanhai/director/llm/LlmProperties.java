package com.shanhai.director.llm;

import java.time.Duration;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * LLM 上游配置。
 *
 * <p><b>key 绝不写在 application.yml 里</b>：本地走 application-local.yml
 * （已 gitignore），CI 与生产走环境变量注入。本次开工的全部意义是
 * "key 不进客户端"，key 若进了 git，就是把一个安全问题换成了更严重的一个。
 */
@ConfigurationProperties(prefix = "shm.llm")
public record LlmProperties(
        String apiKey,
        String baseUrl,
        String model,
        Integer timeoutSeconds
) {
    /**
     * 是否真的可用。
     *
     * <p><b>不能只看非空</b> —— 这是踩坑 #20 的服务端版。那次客户端的
     * {@code IsAvailable()} 只判 {@code !ApiKey.IsEmpty()}，结果测试时随手填的
     * 占位值 {@code "1"} 让 Provider 被选中，此后每层都发一次注定 401 的请求。
     *
     * <p>这里还多一层：Spring 的 {@code ${SHM_LLM_API_KEY:}} 空值兜底与"未配置"
     * 是两种语义 —— 环境变量没设时它给的是空字符串而不是 null。
     * 两者都必须当成不可用。
     *
     * <p>占位值的识别只做到"明显不是 key"的程度（太短、常见占位词）。
     * 真正的失效判断靠上游返回 401/403 —— 见 LlmClient 的自我停用。
     */
    public boolean usable() {
        if (apiKey == null || apiKey.isBlank()) {
            return false;
        }
        String k = apiKey.trim();
        // 真实的 key 都远长于此；"1"/"test"/"todo" 这类占位值一律拒
        if (k.length() < 16) {
            return false;
        }
        String lower = k.toLowerCase();
        return !lower.equals("changeme") && !lower.startsWith("your-") && !lower.startsWith("<");
    }

    public String baseUrlOrDefault() {
        return baseUrl == null || baseUrl.isBlank() ? "https://api.deepseek.com/v1" : baseUrl;
    }

    public String modelOrDefault() {
        return model == null || model.isBlank() ? "deepseek-chat" : model;
    }

    /**
     * 上游超时。
     *
     * <p>必须小于客户端 FSHMRemoteProvider 的 12 秒，否则客户端先放弃、
     * 这次上游调用就成了纯浪费（花了钱没人要结果）。
     * 三者的大小关系：本超时 &lt; 客户端超时 &lt; 玩法层最大等待。
     */
    public Duration timeout() {
        int s = timeoutSeconds == null ? 10 : Math.max(1, timeoutSeconds);
        return Duration.ofSeconds(s);
    }
}
