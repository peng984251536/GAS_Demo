#pragma once
#include "GameplayEffectComponent.h"
#include "CC_BuffRulesComponent.generated.h"
class UCC_BuffBehavior;

/**
 * GE 的只读规则配置组件；描述模板列表和死亡清理策略，不承担具体机制。
 * 配置对象由 GE 共享，不能在此保存层数、计时器或本次射击状态。
 */
UCLASS(DisplayName = "CC Buff Rules")

class GAS_DEMO_API UCC_BuffRulesComponent : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:

	
	/**
	 * 内嵌 Behavior 模板；服务器为每个 ActiveGE 复制一组，不直接启用模板自身。
	 */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Buff")
	TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors;
	/**
	 * 死亡时是否移除整个 GE；为 false 时保留 GE，但规则被暂停。
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Buff")
	bool bRemoveOnDeath = true;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
