#pragma once
#include "Abilities/GameplayAbility.h"
#include "CC_Dash.generated.h"

class UAbilityTask_ApplyRootMotionConstantForce;

/** 临时授予的移动能力；GAS 负责预测、复制和撤回时取消任务。 */
UCLASS()

class GAS_DEMO_API UCC_Dash : public UGameplayAbility
{
    GENERATED_BODY()
  public:
    UCC_Dash();

  protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "1")) float DashSpeed = 1800.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01"))
    float DashDuration = 0.2f;
    /**
     * 使用预测根运动执行短冲刺；不修改永久移速，也不依赖闪避蒙太奇。
     * @param Handle GAS 能力句柄。
     * @param ActorInfo 当前施放者；必须是存活且可以移动的项目角色。
     * @param ActivationInfo GAS 预测与激活信息。
     * @param TriggerEventData 可选事件数据，本能力通常由输入按标签激活。
     */
    virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                 FGameplayAbilityActivationInfo ActivationInfo,
                                 const FGameplayEventData* TriggerEventData) override;
    /**
     * 结束或撤回冲刺；父类结束 AbilityTask 并移除其根运动源。
     * @param Handle GAS 能力句柄。
     * @param ActorInfo 当前施放者。
     * @param ActivationInfo 本次激活信息。
     * @param bReplicateEndAbility 是否复制能力结束。
     * @param bWasCancelled 是否由死亡、Buff 撤回或外部取消导致。
     */
    virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                            FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
                            bool bWasCancelled) override;

  private:
    UFUNCTION() void OnDashFinished();
    UPROPERTY(Transient) TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> DashTask;
};
