// 表现层桥接：接收 Cue 数值与标签，转换治疗符号后提交给飘字子系统；此处不执行扣血。
#include "GameplayCues/CC_GameplayCue_HitImpact.h"

#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_HitImpact, "GameplayCue.Combat.HitImpact");

// 表现层用这两个标签区分暴击与治疗；由 UCC_DamageTextStatics 在发 Cue 时挂上。
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Critical, "GameplayCue.DamageText.Critical");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_Heal, "GameplayCue.DamageText.Heal");

// 注册一次性受击 Cue 标签，使 ASC 的 ExecuteGameplayCue 能找到此表现类。
UCC_GameplayCue_HitImpact::UCC_GameplayCue_HitImpact()
{
    GameplayCueTag = TAG_Cue_HitImpact;
    GameplayCueName = GameplayCueTag.GetTagName();
}

// 只响应一次性的 Executed 事件，不处理持续效果的添加或移除。
bool UCC_GameplayCue_HitImpact::HandlesEvent(EGameplayCueEvent::Type EventType) const
{
    return EventType == EGameplayCueEvent::Executed;
}

// 过滤无效目标和专用服务器，分别处理特效、声音与飘字；返回值只反映特效/声音播放结果。
bool UCC_GameplayCue_HitImpact::OnExecute_Implementation(
    AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
    if (!IsValid(MyTarget) || !MyTarget->GetWorld()
        || MyTarget->GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    FVector Location;
    FVector Normal = FVector(Parameters.Normal).GetSafeNormal();
    if (const FHitResult* Hit = Parameters.EffectContext.GetHitResult())
    {
        // HitResult 明确提供命中位置，即使命中点位于世界原点也使用该位置。
        Location = Hit->ImpactPoint;
        Normal = Hit->ImpactNormal.GetSafeNormal();
    }
    else if (!Parameters.Location.IsNearlyZero() || !Normal.IsNearlyZero())
    {
        // 显式位置为世界原点时，同时提供非零法线，才能区别于未指定位置的事件。
        Location = Parameters.Location;
    }
    else
    {
        // 仅携带标签、没有位置的事件，回退到目标包围盒中心播放特效。
        FVector Extent;
        MyTarget->GetActorBounds(true, Location, Extent);
    }

    if (Normal.IsNearlyZero())
    {
        const AActor* Instigator = Parameters.Instigator.Get();
        if (!IsValid(Instigator))
        {
            Instigator = Parameters.EffectContext.GetOriginalInstigator();
        }
        Normal = IsValid(Instigator)
            ? (Instigator->GetActorLocation() - MyTarget->GetActorLocation()).GetSafeNormal()
            : MyTarget->GetActorForwardVector();
        if (Normal.IsNearlyZero())
        {
            Normal = FVector::ForwardVector;
        }
    }

    Location += Normal * SurfaceOffset;
    const FRotator Rotation = (Normal.Rotation().Quaternion()
        * RotationOffset.Quaternion()).Rotator();

    bool bPlayed = false;
    if (IsValid(HitEffect))
    {
        bPlayed = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            MyTarget, HitEffect, Location, Rotation, FVector(EffectScale),
            true, true, ENCPoolMethod::None, true) != nullptr;
    }
    if (IsValid(HitSound))
    {
        UGameplayStatics::PlaySoundAtLocation(MyTarget, HitSound, Location, SoundVolume);
        bPlayed = true;
    }

    // --- 伤害飘字 ---
    // 与受击特效分开处理：即使没有 Niagara/Sound，只要有伤害值也应该显示数字，
    // 所以这一步不参与上面的 bPlayed 判定。
    if (bShowDamageText && !FMath::IsNearlyZero(Parameters.RawMagnitude))
    {
        if (UCC_DamageTextSubsystem* DamageText =
            MyTarget->GetWorld()->GetSubsystem<UCC_DamageTextSubsystem>())
        {
            // 锚点用 Parameters.Location（UCC_DamageTextStatics 写入的是头顶），
            // 而不是上面算出的命中点——数字贴在身上不易读，飘在头顶才对。
            FVector TextAnchor = Parameters.Location;
            if (TextAnchor.IsNearlyZero())
            {
                FVector Extent;
                MyTarget->GetActorBounds(true, TextAnchor, Extent);
                TextAnchor.Z += Extent.Z * 0.6f;
            }

            // 样式由 Cue 标签决定，与 UCC_DamageTextStatics 的约定对应。
            ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal;
            if (Parameters.AggregatedSourceTags.HasTagExact(TAG_Cue_Critical))
            {
                Style = ECC_DamageTextStyle::Critical;
            }
            else if (Parameters.AggregatedSourceTags.HasTagExact(TAG_Cue_Heal))
            {
                Style = ECC_DamageTextStyle::Heal;
            }

            // 治疗以负数传入，与子系统的正负约定一致（正数伤害、负数治疗）。
            const float Amount = (Style == ECC_DamageTextStyle::Heal)
                ? -FMath::Abs(Parameters.RawMagnitude)
                : FMath::Abs(Parameters.RawMagnitude);

            DamageText->ReportHit(TextAnchor, Amount, Style);
        }
    }

    return bPlayed;
}
