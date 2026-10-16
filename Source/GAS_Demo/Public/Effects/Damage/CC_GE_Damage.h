#pragma once

#include "GameplayEffect.h"
#include "CC_GE_Damage.generated.h"

/**
 * 项目唯一的伤害 GE（Instant）。由 UCC_DamageStatics 创建 Spec，技能不要直接使用。
 *  - Execution：UCC_DamageExecution 计算伤害，输出到 Damage 中间属性；
 *  - Tag Requirements：目标带 Status.DamageImmunity 时不生效；
 *  - GameplayCue：GameplayCue.Combat.DamageText，数值取 Damage 中间属性（理论伤害）。
 * 想在编辑器里改 Cue，可新建继承它的 GE 资产，并在 项目设置 → CC Damage 中指定。
 * 周期伤害（DoT）可派生子类配置 Duration/Period，每跳独立结算并飘字。
 */
UCLASS()
class GAS_DEMO_API UCC_GE_Damage : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UCC_GE_Damage();
};
