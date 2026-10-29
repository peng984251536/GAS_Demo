#include "Data/Buff/CC_BuffBehavior_SplitArrow.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Data/Buff/CC_BuffDebug.h"
#include "Actors/Projectiles/CC_ProjectileFireContext.h"

UCC_BuffBehavior_SplitArrow::UCC_BuffBehavior_SplitArrow()
{
    HandledHooks.AddTag(CCBuffTags::ProjectileFire);
}

/**
 * 只处理发射决策点；将当前层数折算为数量增量，不生成箭，也不修改共享模板。
 * @param Hook 预期为 Hook.Projectile.Fire。
 * @param Context 当前这一轮射击参数，生命周期只覆盖一次放箭。
 */
void UCC_BuffBehavior_SplitArrow::ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context)
{
    if (Hook != CCBuffTags::ProjectileFire)
    {
        return;
    }
    FCC_ProjectileFireContext* Fire = Context.GetMutablePtr<FCC_ProjectileFireContext>();
    if (!Fire || !FMath::IsFinite(SpreadAngle) || !FMath::IsFinite(DamagePerArrowRatio))
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[SplitArrow] Invalid context/config Owner=%s Instance=%d"),
               *GetNameSafe(GetContext().Owner.Get()), GetContext().InstanceId);
        return;
    }
    const int32 Before = Fire->ProjectileCount;
    /**
     * 使用较宽整数做中间计算，并把最终数量限制到 32，防止异常配置造成溢出或大量生成。
     */
    const int64 Count = static_cast<int64>(Before) + static_cast<int64>(FMath::Clamp(ExtraArrows, 0, 31)) *
                                                         FMath::Clamp(GetContext().StackCount, 0, 32);
    Fire->ProjectileCount = static_cast<int32>(FMath::Clamp<int64>(Count, 1, 32));
    Fire->SpreadAngle = FMath::Max(Fire->SpreadAngle, FMath::Clamp(SpreadAngle, 0.f, 180.f));
    /**
     * 同一实例无论几层都只乘一次 0.6；两个独立来源实例则依次相乘为 0.36。
     * 数量按层数相加，散射取最大，因此多个规则可以组合而无需相互识别。
     */
    Fire->Payload.DamageMultiplier *= FMath::Clamp(DamagePerArrowRatio, 0.f, 1.f);

	
    UE_LOG(LogCCBuff, Verbose, TEXT("[SplitArrow] Owner=%s Instance=%d Stacks=%d Count=%d->%d Spread=%.1f Ratio=%.3f"),
           *GetNameSafe(GetContext().Owner.Get()), GetContext().InstanceId, GetContext().StackCount, Before,
           Fire->ProjectileCount, Fire->SpreadAngle, Fire->Payload.DamageMultiplier);
}
