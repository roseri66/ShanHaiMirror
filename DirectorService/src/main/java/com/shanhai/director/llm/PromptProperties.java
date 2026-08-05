package com.shanhai.director.llm;

import org.springframework.boot.context.properties.ConfigurationProperties;

/**
 * prompt.yaml 的绑定。
 *
 * <p>prompt 文本不写在 Java 里，是为了让"改 prompt"不需要重新编译，
 * 也让它在仓库里是一个能直接读的文本文件——面试时打开 prompt.yaml
 * 比翻 .java 里的字符串拼接直观得多。
 */
@ConfigurationProperties(prefix = "shm.prompt")
public record PromptProperties(
        String system,
        Double temperature
) {
    public double temperatureOrDefault() {
        return temperature == null ? 0.7 : temperature;
    }
}
