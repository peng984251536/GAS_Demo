#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageTextStatics.generated.h"

class UAbilitySystemComponent;

/**
 * 独立伤害/治疗飘字工具：只显示指定数值，不应用 GE、不修改 Health，也不播放受击特效。
 * 用于调试、特殊表现或已有独立扣血流程。常规 ApplyDamage/ApplyHealing 已自动发数字，
 * 同一次结算不要再调用本工具，否则会重复显示。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageTextStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /**
     * 独立显示指定数字，不应用 GE、不修改生命、不播放受击特效；常规伤害已自动飘字时不要重复调用。
     * @param TargetASC 目标 ASC；需要有效 Avatar，伤害/治疗入口还需项目属性集。
     * @param Amount 指定数字，取绝对值；治疗由 Heal 样式明确指定。
     * @param Style 普通、暴击或治疗样式，决定颜色、字号及治疗加号。
     * @param SourceASC 可选来源 ASC；省略时显示给收到 Cue 的本地玩家。
     */
    UFUNCTION(BlueprintCallable, Category="GAS|Damage Text", meta=(AdvancedDisplay="SourceASC"))
    static void ShowDamageText(UAbilitySystemComponent* TargetASC, float Amount,
        ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal, UAbilitySystemComponent* SourceASC = nullptr);

    /**
     * 根据已观测到的生命差值显示数字；上升为治疗，下降为伤害，零差值不显示。
     * @param TargetASC 目标 ASC；需要有效 Avatar，伤害/治疗入口还需项目属性集。
     * @param HealthBefore 调用方观测到的结算前生命值。
     * @param HealthAfter 调用方观测到的结算后生命值。
     * @param bCritical 生命下降时是否使用暴击样式；生命上升仍使用治疗样式。
     * @param SourceASC 可选来源 ASC；省略时显示给收到 Cue 的本地玩家。
     */
    UFUNCTION(BlueprintCallable, Category="GAS|Damage Text", meta=(AdvancedDisplay="SourceASC"))
    static void ShowDamageTextFromDelta(UAbilitySystemComponent* TargetASC, float HealthBefore,
        float HealthAfter, bool bCritical = false, UAbilitySystemComponent* SourceASC = nullptr);
};
