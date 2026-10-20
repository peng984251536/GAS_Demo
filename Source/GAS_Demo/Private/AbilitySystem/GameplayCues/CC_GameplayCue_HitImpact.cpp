// 独立命中特效与声音；此处不处理扣血或伤害数字。
#include "AbilitySystem/GameplayCues/CC_GameplayCue_HitImpact.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Cue_HitImpact, "GameplayCue.Combat.HitImpact");

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

// 过滤无效目标和专用服务器后播放特效与声音；伤害数字由 DamageText Cue 负责。
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

    return bPlayed;
}
