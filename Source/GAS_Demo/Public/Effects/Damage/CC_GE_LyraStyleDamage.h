#pragma once

#include "GameplayEffect.h"
#include "CC_GE_LyraStyleDamage.generated.h"

/**
 * 内置 Instant 伤害 GE，负责配置一次伤害应用使用的 Execution。
 * 技能通过 UCC_DamageStatics::ApplyDamage 创建此类的 Spec；计算实现位于 Damage 目录。
 * 只登记一条伤害 Execution，不额外添加 Health 扣减 Modifier，避免重复扣血。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_LyraStyleDamage : public UGameplayEffect
{
    GENERATED_BODY()
public:
    /**
     * 配置 Instant 生命周期和伤害 Execution，不额外添加 Health 扣减 Modifier。
     */
    UCC_GE_LyraStyleDamage();
};
