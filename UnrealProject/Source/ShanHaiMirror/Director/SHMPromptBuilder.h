#pragma once

#include "CoreMinimal.h"
#include "SHMDirectorTypes.h"

// ============================================================================
// Prompt 构建 —— Context → OpenAI 兼容 chat completions 请求体
//
// 设计要点：**Prompt 里注入的候选集就是 Context 里那个安全集**。
// LLM 只能在已经过滤过的原型/规则里挑，加上后面的四道护栏，形成两层约束：
//   第一层（这里）：不给它越界的选项
//   第二层（护栏）：即便它越界了也进不了玩法层
//
// Prompt 明令禁止输出任何数值——这是 D-15 在自然语言层的表述，
// 与类型系统（FDirectorIntent 装不下数值）、解析器（丢弃数值字段）三重保险。
//
// 纯函数：同 Context 必得同 prompt，可单测。
// ============================================================================
class SHANHAIMIRROR_API FSHMPromptBuilder
{
public:
	// 完整 HTTP 请求体（含 model / messages / temperature）
	static FString BuildRequestBody(const FDirectorContext& Context, const FString& Model);

	// 两段 prompt 单独暴露，便于测试与调试
	static FString BuildSystemPrompt();
	static FString BuildUserPrompt(const FDirectorContext& Context);
};
