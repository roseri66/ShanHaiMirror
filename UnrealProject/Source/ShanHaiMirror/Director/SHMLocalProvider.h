#pragma once

#include "CoreMinimal.h"
#include "SHMAIProvider.h"

// ============================================================================
// 本地规则表 Provider —— 降级终点，单独就是一个完整可玩的导演
//
// 决策完全确定性（不掷随机数）：同一画像必得同一决策，可单测。
// "重复游玩的变化"由 W3 遭遇生成器在原型权重下随机抽具体怪实现，
// 不在决策层引入随机——这是 TDD §2.5 就定下的分工。
// ============================================================================
class SHANHAIMIRROR_API FSHMLocalProvider : public ISHMAIProvider
{
public:
	virtual FDirectorIntent RequestIntent(const FDirectorContext& Context) override;
	virtual FString GetProviderName() const override { return TEXT("Local"); }
};
