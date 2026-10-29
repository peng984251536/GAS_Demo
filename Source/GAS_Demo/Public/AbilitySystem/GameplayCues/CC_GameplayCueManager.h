#pragma once
#include "GameplayCueManager.h"
#include "CC_GameplayCueManager.generated.h"

//TODO- 这个类有点抽象，我不好理解
/**
 * 项目 Cue 管理器：让原生 C++ 写的 GameplayCue（伤害/治疗数字）不需要蓝图资产也能被找到。
 * GAS 只扫描资产目录，扫描不到纯 C++ 的 Notify；编辑器重新扫描还会清掉手动注册，所以在路由时按需补注册。
 * 以后若改为在 /Game 下建继承这些类的蓝图 Cue 资产，可以删除这个类和 DefaultGame.ini 里的配置。
 */
UCLASS()
class GAS_DEMO_API UCC_GameplayCueManager : public UGameplayCueManager
{
	GENERATED_BODY()
public:
	virtual void RouteGameplayCue(AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
		const FGameplayCueParameters& Params, EGameplayCueExecutionOptions Options = EGameplayCueExecutionOptions::Default) override;
};
