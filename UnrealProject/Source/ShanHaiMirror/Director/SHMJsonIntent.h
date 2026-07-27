#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

class FJsonObject;

// ============================================================================
// Intent 的 JSON 序列化 —— LLM 与回放 Provider 共用的同一套解析
//
// **这是"不信任 LLM 输出"的第一道关卡**（护栏是第二道）：
//   · 畸形 JSON / 缺字段 → 报失败，调用方降级，绝不抛异常绝不崩
//   · 多余字段 → 忽略
//   · **数值字段一律丢弃**——LLM 若擅自输出 multiplier，解析器直接不认。
//     数值只由 RuleResolver 查表产生（D-15），这里是类型系统之外的第二重保险
//   · 幻觉标签（表里不存在的 Enemy.Xxx）照常解析出来，交给 Schema 护栏拒绝——
//     解析器不做业务判断，只保证"结构安全"，职责边界清晰
//
// 纯函数，不碰世界不碰网络，可单测。
// ============================================================================
class SHANHAIMIRROR_API FSHMJsonIntent
{
public:
	// JSON 文本 → Intent。bOutOk=false 表示不可用（调用方须降级）
	static FDirectorIntent ParseFromJson(const FString& Json, bool& bOutOk);

	// 已解析的 JSON 对象 → Intent（决策日志读取时复用，避免二次解析）
	static FDirectorIntent ParseFromJsonObject(const TSharedPtr<FJsonObject>& Obj, bool& bOutOk);

	// Intent → JSON 对象（决策日志写 rawIntent 用）
	static TSharedPtr<FJsonObject> ToJsonObject(const FDirectorIntent& Intent);

	// 挑战等级 ↔ 字符串（日志可读性优先于紧凑性）
	static FString      ChallengeLevelToString(EChallengeLevel Level);
	static EChallengeLevel ChallengeLevelFromString(const FString& Str);
};
