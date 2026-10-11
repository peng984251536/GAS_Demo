#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "CC_DamageExecution.generated.h"

/**
 * 项目唯一伤害 Execution：捕获战斗属性、确定暴击、调用纯公式并输出正 IncomingDamage。
 * 直接使用 UCC_AttributeSet，不再拆通用父类、属性适配子类和输入转发钩子。
 * 只计算应造成的伤害；实际生命修改、死亡监听和数字反馈仍由各自的结算/表现层处理。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageExecution : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()
public:
    /**
     * 登记固定的项目属性捕获；攻击方属性采用快照，目标属性在执行时读取。
     */
    UCC_DamageExecution();

    /**
     * 在目标权威端完成一次伤害计算，输出中转伤害，不直接裁剪或修改 Health。
     * @param Params 本次 Spec、双方 ASC、标签与属性捕获的求值参数。
     * @param Output 输出集合，加入正 IncomingDamage 供属性集消费。
     */
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Params,
        FGameplayEffectCustomExecutionOutput& Output) const override;
private:
    /** 攻击力快照，包含 GAS 已聚合的攻击 Buff。 */
    UPROPERTY() FGameplayEffectAttributeCaptureDefinition AttackCapture;
    /** 目标固定防御，不快照。 */
    UPROPERTY() FGameplayEffectAttributeCaptureDefinition DefenseCapture;
    /** 攻击方增伤比例快照。 */
    UPROPERTY() FGameplayEffectAttributeCaptureDefinition BonusCapture;
    /** 目标承伤倍率，不快照。 */
    UPROPERTY() FGameplayEffectAttributeCaptureDefinition IncomingCapture;
    /** 攻击方暴击率快照。 */
    UPROPERTY() FGameplayEffectAttributeCaptureDefinition ChanceCapture;
    /** 攻击方暴击总倍率快照。 */
    UPROPERTY() FGameplayEffectAttributeCaptureDefinition CriticalCapture;
};
