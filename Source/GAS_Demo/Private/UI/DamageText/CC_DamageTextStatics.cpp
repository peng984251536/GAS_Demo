#include "UI/DamageText/CC_DamageTextStatics.h"
#include "AbilitySystemComponent.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "GameplayCues/CC_GameplayCue_DamageText.h"
#include "GameFramework/Actor.h"

/**
 * 独立显示指定数字，不应用 GE、不修改生命、不播放受击特效；常规伤害已自动飘字时不要重复调用。
 * @param TargetASC 目标 ASC；需要有效 Avatar，伤害/治疗入口还需项目属性集。
 * @param Amount 指定数字，取绝对值；治疗由 Heal 样式明确指定。
 * @param Style 普通、暴击或治疗样式，决定颜色、字号及治疗加号。
 * @param SourceASC 可选来源 ASC；省略时显示给收到 Cue 的本地玩家。
 */
void UCC_DamageTextStatics::ShowDamageText(UAbilitySystemComponent* TargetASC, float Amount,
    ECC_DamageTextStyle Style, UAbilitySystemComponent* SourceASC)
{
    if (!IsValid(TargetASC) || !IsValid(TargetASC->GetAvatarActor())
        || !FMath::IsFinite(Amount) || FMath::IsNearlyZero(Amount)) return;
    if (Style != ECC_DamageTextStyle::Normal && Style != ECC_DamageTextStyle::Critical && Style != ECC_DamageTextStyle::Heal) return;
    AActor* Source = IsValid(SourceASC) ? SourceASC->GetAvatarActor() : nullptr;
    FGameplayEffectContextHandle Context(new FCC_GameplayEffectContext());
    Context.AddInstigator(Source, Source);
    FCC_GameplayEffectContext::GetMutable(Context)->bCritical = Style == ECC_DamageTextStyle::Critical;
    FGameplayCueParameters Params;
    Params.EffectContext = Context;
    Params.Instigator = Source;
    Params.EffectCauser = Source;
    Params.RawMagnitude = Style == ECC_DamageTextStyle::Heal ? -FMath::Abs(Amount) : FMath::Abs(Amount);
    FVector Extent;
    TargetASC->GetAvatarActor()->GetActorBounds(true, Params.Location, Extent);
    Params.Location.Z += Extent.Z + 20.f;
    // 显式显示工具未提供来源时，不能因目标是敌人而把本地数字全部过滤掉。
    if (!Source) Params.AggregatedSourceTags.AddTag(CCDamageCueTags::ManualDisplay);
    if (TargetASC->IsOwnerActorAuthoritative())
        TargetASC->ExecuteGameplayCue(CCDamageCueTags::DamageText, Params);
    else
        // 非预测客户端直接 ExecuteGameplayCue 可能不执行；显式调用本地事件不发送 RPC。
        // TODO
        TargetASC->InvokeGameplayCueEvent(CCDamageCueTags::DamageText, EGameplayCueEvent::Executed, Params);
}

/**
 * 根据已观测到的生命差值显示数字；上升为治疗，下降为伤害，零差值不显示。
 * @param TargetASC 目标 ASC；需要有效 Avatar，伤害/治疗入口还需项目属性集。
 * @param HealthBefore 调用方观测到的结算前生命值。
 * @param HealthAfter 调用方观测到的结算后生命值。
 * @param bCritical 生命下降时是否使用暴击样式；生命上升仍使用治疗样式。
 * @param SourceASC 可选来源 ASC；省略时显示给收到 Cue 的本地玩家。
 */
void UCC_DamageTextStatics::ShowDamageTextFromDelta(UAbilitySystemComponent* TargetASC,
    float HealthBefore, float HealthAfter, bool bCritical, UAbilitySystemComponent* SourceASC)
{
    if (!FMath::IsFinite(HealthBefore) || !FMath::IsFinite(HealthAfter)) return;
    const float Delta = HealthBefore - HealthAfter;
    if (!FMath::IsFinite(Delta) || FMath::IsNearlyZero(Delta)) return;
    const auto Style = Delta < 0.f ? ECC_DamageTextStyle::Heal
        : (bCritical ? ECC_DamageTextStyle::Critical : ECC_DamageTextStyle::Normal);
    ShowDamageText(TargetASC, Delta, Style, SourceASC);
}
