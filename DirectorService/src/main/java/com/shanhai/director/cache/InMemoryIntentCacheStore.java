package com.shanhai.director.cache;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Component;

import com.shanhai.director.api.DirectorIntent;

/**
 * 内存实现的缓存后端。
 *
 * <p>单实例部署下这就是对的：一个 ConcurrentHashMap，没有网络往返、
 * 没有序列化、没有多一个要运维的进程。多实例部署时才需要 Redis，
 * 而那个部署形态现在不存在。
 *
 * <p><b>内存方案必须自己管淘汰</b>——Redis 有 TTL，这里没有。
 * 不管的话指纹会随游玩累积，一个长期运行的服务最终 OOM。
 * 这是选内存实现要自己付的代价，写在这里免得被当成"忘了"。
 */
@Component
public class InMemoryIntentCacheStore implements IntentCacheStore {

    private static final Logger log = LoggerFactory.getLogger(InMemoryIntentCacheStore.class);

    /** 指纹数量上限。超过就整体清空——见 {@link #evictIfLarge()} 的说明。 */
    private static final int MAX_FINGERPRINTS = 5_000;

    private final Map<String, List<DirectorIntent>> store = new ConcurrentHashMap<>();

    @Override
    public List<DirectorIntent> get(String fingerprint) {
        List<DirectorIntent> variants = store.get(fingerprint);
        return variants == null ? List.of() : List.copyOf(variants);
    }

    @Override
    public void append(String fingerprint, DirectorIntent intent, int maxVariants) {
        // compute 保证同一 key 上的原子性：并发下不会超出 maxVariants。
        // 换 Redis 时这一点要用 Lua 或 WATCH 保住，否则会超额缓存。
        store.compute(fingerprint, (k, existing) -> {
            List<DirectorIntent> list = existing == null ? new ArrayList<>() : new ArrayList<>(existing);
            if (list.size() < maxVariants) {
                list.add(intent);
            }
            return Collections.unmodifiableList(list);
        });
        evictIfLarge();
    }

    @Override
    public int size() {
        return store.size();
    }

    @Override
    public void clear() {
        store.clear();
    }

    /**
     * 粗暴淘汰：超阈值就整体清空。
     *
     * <p>精确的 LRU 在这个量级上不值得——清空的后果只是大家重新走一次 LLM，
     * 而缓存本来就是"省钱"而非"正确性"的东西。
     * 用 Redis 时这段可以整个删掉，交给 TTL。
     */
    private void evictIfLarge() {
        if (store.size() > MAX_FINGERPRINTS) {
            log.info("缓存指纹数 {} 超阈值，清空重来（内存实现无 TTL，必须自己淘汰）", store.size());
            store.clear();
        }
    }
}
