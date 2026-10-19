#include "Character/Combat/CC_ProjectileFireContext.h"
#include "Character/Combat/CC_ArrowProjectile.h"
#include "Data/Buff/CC_BuffDebug.h"

/**
 * 规则全部修改完毕后统一校验并展开，不在各个 Behavior 中分别计算散射方向。
 * @param OutDirections 输出本次齐射的单位方向；非法配置会清空输出并中止发射。
 */
void FCC_ProjectileFireContext::Resolve(TArray<FVector>& OutDirections) const
{
    OutDirections.Reset();

    if (!ProjectileClass || Origin.ContainsNaN() || AimDirection.ContainsNaN() || AimDirection.IsNearlyZero() ||
        !FMath::IsFinite(Speed) || Speed <= 0.f || !FMath::IsFinite(SpreadAngle) ||
        !FMath::IsFinite(Payload.SkillScale) || Payload.SkillScale < 0.f || !FMath::IsFinite(Payload.FlatDamage) ||
        Payload.FlatDamage < 0.f || !FMath::IsFinite(Payload.DamageMultiplier) || Payload.DamageMultiplier < 0.f)
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[Fire][Resolve] Invalid projectile parameters; volley skipped."));
        return;
    }

    const int32 Count = FMath::Clamp(ProjectileCount, 1, 32);
    const float Angle = FMath::Clamp(SpreadAngle, 0.f, 180.f);
    const FVector Forward = AimDirection.GetSafeNormal();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        /**
         * 总夹角均分到各支箭：3 支、30 度得到 -15/0/+15 度；只有 1 支时始终保持原方向。
         */
        const float Offset = Count == 1 ? 0.f : -Angle * 0.5f + Angle * Index / (Count - 1);
        OutDirections.Add(Forward.RotateAngleAxis(Offset, FVector::UpVector).GetSafeNormal());
    }
}
