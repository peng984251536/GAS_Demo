#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "CC_DamageExecution.generated.h"

/**
 * 伤害计算：读取攻防属性 → 判定暴击 → 纯公式 → 输出 Damage 中间属性。不扣血、不做表现。
 * 攻击方属性在创建 Spec 时快照，目标属性在执行时读取。
 * 属性按 Spec 标签（伤害类型、来源标签）求值，所以 GE 修正器上的 Source/Target Tag Requirements 会自动生效。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	UCC_DamageExecution();
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
		FGameplayEffectCustomExecutionOutput& Output) const override;
};
