// 一次性受击表现入口：可同时播放特效、声音和飘字；仅处理 Executed 类型的 GameplayCue。
#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "CC_GameplayCue_HitImpact.generated.h"

class UNiagaraSystem;
class USoundBase;

/** 一次性受击表现，通过目标 ASC 的 ExecuteGameplayCue 触发；不承担伤害结算。 */
UCLASS(Blueprintable, meta = (DisplayName = "CC Hit Impact Gameplay Cue"))
class GAS_DEMO_API UCC_GameplayCue_HitImpact : public UGameplayCueNotify_Static
{
    GENERATED_BODY()

public:
    UCC_GameplayCue_HitImpact();

    virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
    virtual bool OnExecute_Implementation(AActor* MyTarget,
        const FGameplayCueParameters& Parameters) const override;

    /** 使用非循环 Niagara 特效，播放结束后生成的组件自动销毁。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact")
    TObjectPtr<UNiagaraSystem> HitEffect;

    /** 可选的一次性音效；空间衰减由音效资产自身的衰减设置决定。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact", meta = (ClampMin = "0.01"))
    float EffectScale = 0.65f;

    /** 先让特效的局部 +X 朝向命中法线，再叠加该旋转修正。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact")
    FRotator RotationOffset = FRotator::ZeroRotator;

    /** 沿法线把受击特效向表面外推，单位厘米；不改变飘字锚点。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact", meta = (ClampMin = "0.0"))
    float SurfaceOffset = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact", meta = (ClampMin = "0.0"))
    float SoundVolume = 0.7f;

    /**
     * 是否顺便弹出伤害飘字。
     *
     * 飘字的数值通过 FGameplayCueParameters::RawMagnitude 传入，
     * 由技能侧在 ApplyGameplayEffectSpecToTarget 之后用
     * UCC_DamageTextStatics::ApplyDamageWithText 或 ShowDamageText 设置。
     *
     * 关闭此项可以只保留受击特效，不显示数字（例如环境伤害、调试场景）。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Impact|Damage Text")
    bool bShowDamageText = true;
};
