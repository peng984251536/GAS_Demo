#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Damage/CC_DamageTypes.h"
#include "CC_DamageStatics.generated.h"
class UAbilitySystemComponent;
/**
 * 技能侧统一的伤害/治疗入口，C++ 和蓝图均可调用。
 * 在服务器确认命中后调用；只负责构造和应用 GE，结算结果会自动触发飘字。
 * 使用内置 CC_GE_LyraStyleDamage / CC_GE_Healing，不需要另传旧 GE 或 SetByCaller Tag。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /**
     * 在权威端构造并应用即时伤害 GE，后续计算、扣血和自动飘字由结算链路完成。
     * @param SourceASC 攻击者 ASC；必须有效、初始化 Avatar，并具有项目属性集。
     * @param TargetASC 受击者 ASC；必须有效、初始化 Avatar，并具有项目属性集。
     * @param Request 服务器认可的技能系数、固定伤害、命中信息与暴击策略。
     * @return GE 应用状态；Applied 表示接受应用，不保证造成正伤害。
     */
    UFUNCTION(BlueprintCallable, Category="GAS|Damage")
    static ECC_DamageApplyStatus ApplyDamage(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
        const FCC_DamageRequest& Request);
    /**
     * 在权威端应用正数固定治疗，自动限制恢复量并显示实际治疗数字。
     * @param SourceASC 治疗来源 ASC；必须有效、初始化 Avatar，并具有项目属性集。
     * @param TargetASC 被治疗者 ASC；必须有效、初始化 Avatar，并具有项目属性集。
     * @param Amount 非负固定治疗量，实际恢复量受目标缺失生命限制。
     * @return GE 应用状态；零治疗或满血可能不产生生命变化。
     */
    UFUNCTION(BlueprintCallable, Category="GAS|Damage")
    static ECC_DamageApplyStatus ApplyHealing(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, float Amount);
};
