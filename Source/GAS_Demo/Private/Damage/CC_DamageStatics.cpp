#include "Damage/CC_DamageStatics.h"
#include "Effects/Damage/CC_GE_LyraStyleDamage.h"
#include "Effects/Damage/CC_GE_Healing.h"
#include "Damage/CC_DamageTypes.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"

namespace
{
/**
 * 检查 ASC、Avatar 与项目生命属性是否有效，防止使用无效目标构造伤害 Spec。
 * @param ASC 待绑定的项目 ASC；反馈组件传 nullptr 表示解绑。
 * @return 满足必要条件返回 true，否则返回 false。
 */
bool HasVitals(UAbilitySystemComponent* ASC)
{
    return IsValid(ASC) && IsValid(ASC->GetAvatarActor()) &&
           ASC->HasAttributeSetForAttribute(UCC_AttributeSet::GetHealthAttribute());
}
}

/**
 * 在权威端构造并应用即时伤害 GE，后续计算、扣血和自动飘字由结算链路完成。
 * @param SourceASC 攻击者 ASC；必须有效、初始化 Avatar，并具有项目属性集。
 * @param TargetASC 受击者 ASC；必须有效、初始化 Avatar，并具有项目属性集。
 * @param Request 服务器认可的技能系数、固定伤害、命中信息与暴击策略。
 * @return GE 应用状态；Applied 表示接受应用，不保证造成正伤害。
 */
ECC_DamageApplyStatus UCC_DamageStatics::ApplyDamage(UAbilitySystemComponent* SourceASC,
                                                     UAbilitySystemComponent* TargetASC,
                                                     const FCC_DamageRequest& Request)
{
    if (!HasVitals(SourceASC) || !HasVitals(TargetASC))
    {
        return ECC_DamageApplyStatus::InvalidInput;
    }
    if (!SourceASC->IsOwnerActorAuthoritative() || !TargetASC->IsOwnerActorAuthoritative())
    {
        return ECC_DamageApplyStatus::NotAuthority;
    }
    return ApplyDamage(TargetASC, PrepareDamage(SourceASC, Request));
}

/**
 * 在发射时创建 Spec，捕获攻击方属性；此处尚未找到命中目标，因此不在这里扣血。
 * @param SourceASC 服务器上的攻击者 ASC。
 * @param Request 本次已合并的技能参数与来源标签。
 * @param SourceObject 对应这次攻击的对象，例如正在生成的箭。
 * @return 供命中阶段使用的伤害快照，无效参数返回空句柄。
 */
FGameplayEffectSpecHandle UCC_DamageStatics::PrepareDamage(UAbilitySystemComponent* SourceASC,
                                                           const FCC_DamageRequest& Request, UObject* SourceObject)
{
    if (!HasVitals(SourceASC) || !SourceASC->IsOwnerActorAuthoritative())
    {
        return {};
    }
    for (float Value :
         {Request.SkillScale, Request.FlatDamage, Request.DistanceMultiplier, Request.WeakPointMultiplier})
    {
        if (!FMath::IsFinite(Value) || Value < 0.f)
        {
            return {};
        }
    }
    if (Request.CriticalPolicy != ECC_CriticalPolicy::Disabled &&
        Request.CriticalPolicy != ECC_CriticalPolicy::Random && Request.CriticalPolicy != ECC_CriticalPolicy::Forced)
    {
        return {};
    }
    // 使用配置的自定义 Globals 分配 Context；若编辑器未重启/配置错误，则返回 InvalidInput。
    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    FCC_GameplayEffectContext* Typed = FCC_GameplayEffectContext::GetMutable(Context);
    if (!Typed)
    {
        return {};
    }

    Typed->DamageType = Request.DamageType;
    Typed->SourceTags = Request.SourceTags;
    Typed->CriticalPolicy = Request.CriticalPolicy;
    Typed->bUseHitLocation = Request.bUseHitLocation;
    if (Request.bHasHitResult)
    {
        Context.AddHitResult(Request.HitResult);
    }
    if (SourceObject)
    {
        Context.AddSourceObject(SourceObject);
    }
    // 此刻捕获攻击方快照；目标防御和承伤倍率由 Execution 在应用时读取。
    // 执行一个伤害应用的effect
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCC_GE_LyraStyleDamage::StaticClass(), 1.f, Context);
    if (!Spec.IsValid())
    {
        return {};
    }

    // 这里只传本次技能输入，不再次乘 Buff；所有伤害参数保持非负。
    Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, Request.SkillScale);
    Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, Request.FlatDamage);
    Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::DistanceMultiplier, Request.DistanceMultiplier);
    Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::HitMultiplier, Request.WeakPointMultiplier);

    return Spec;
}

/**
 * 命中时复制保存的 Spec 和 Context；使用发射时攻击属性，目标防御仍在结算时读取。
 * @param TargetASC 本次被命中的目标 ASC。
 * @param Snapshot 先前 PrepareDamage 创建的项目伤害快照。
 * @param Hit 当前命中信息；为空时保留快照原有的 Context。
 * @return GE 是否接受应用；Applied 不代表一定产生正伤害。
 */
ECC_DamageApplyStatus UCC_DamageStatics::ApplyDamage(UAbilitySystemComponent* TargetASC,
                                                     const FGameplayEffectSpecHandle& Snapshot, const FHitResult* Hit)
{
    if (!HasVitals(TargetASC) || !Snapshot.IsValid() || !Snapshot.Data.IsValid() || !Snapshot.Data->Def ||
        !Snapshot.Data->Def->IsA<UCC_GE_LyraStyleDamage>())
    {
        return ECC_DamageApplyStatus::InvalidInput;
    }
    if (!TargetASC->IsOwnerActorAuthoritative())
    {
        return ECC_DamageApplyStatus::NotAuthority;
    }
    FGameplayEffectSpec Spec(*Snapshot.Data);
    Spec.DuplicateEffectContext();
    if (Hit)
    {
        FGameplayEffectContextHandle Context = Spec.GetContext();
        Context.AddHitResult(*Hit, true);
    }
    return TargetASC->ApplyGameplayEffectSpecToSelf(Spec).WasSuccessfullyApplied() ? ECC_DamageApplyStatus::Applied
                                                                                   : ECC_DamageApplyStatus::Rejected;
}

/**
 * 在权威端应用正数固定治疗，自动限制恢复量并显示实际治疗数字。
 * @param SourceASC 治疗来源 ASC；必须有效、初始化 Avatar，并具有项目属性集。
 * @param TargetASC 被治疗者 ASC；必须有效、初始化 Avatar，并具有项目属性集。
 * @param Amount 非负固定治疗量，实际恢复量受目标缺失生命限制。
 * @return GE 应用状态；零治疗或满血可能不产生生命变化。
 */
ECC_DamageApplyStatus UCC_DamageStatics::ApplyHealing(UAbilitySystemComponent* SourceASC,
                                                      UAbilitySystemComponent* TargetASC, float Amount)
{
    if (!HasVitals(SourceASC) || !HasVitals(TargetASC) || !FMath::IsFinite(Amount) || Amount < 0.f)
    {
        return ECC_DamageApplyStatus::InvalidInput;
    }
    if (!SourceASC->IsOwnerActorAuthoritative() || !TargetASC->IsOwnerActorAuthoritative())
    {
        return ECC_DamageApplyStatus::NotAuthority;
    }
    FGameplayEffectSpecHandle Spec =
        SourceASC->MakeOutgoingSpec(UCC_GE_Healing::StaticClass(), 1.f, SourceASC->MakeEffectContext());
    if (!Spec.IsValid())
    {
        return ECC_DamageApplyStatus::InvalidInput;
    }
    Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::Healing, Amount);

    return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC).WasSuccessfullyApplied()
               ? ECC_DamageApplyStatus::Applied
               : ECC_DamageApplyStatus::Rejected;
}
