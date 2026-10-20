#include "AbilitySystem/Abilities/Mechanics/CC_Dash.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/CC_Tags.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Data/Buff/CC_BuffDebug.h"

/**
 * 按角色保存能力实例，使用 GAS 本地预测；能力标签用于通用查找和防止重复激活。
 * 授予 GA 与激活 GA 是两步，Buff 不替换 PlayerController 中的输入策略。
 */
UCC_Dash::UCC_Dash()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    SetAssetTags(FGameplayTagContainer(CCBuffTags::DashAbility));
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag = CCBuffTags::DashAbility;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(Trigger);
    ActivationOwnedTags.AddTag(CCBuffTags::DashAbility);
    ActivationBlockedTags.AddTag(CCBuffTags::DashAbility);
    ActivationBlockedTags.AddTag(CCTags::Status::Death);
    ActivationBlockedTags.AddTag(CCTags::Status::BeHit);
    ActivationBlockedTags.AddTag(CCTags::CCAbilities::Attack);
}

void UCC_Dash::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                               FGameplayAbilityActivationInfo ActivationInfo,
                               const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(GetAvatarActorFromActorInfo());
    if (!IsValid(Character) || !Character->IsAlive() || !Character->GetCharacterMovement() ||
        Character->GetCharacterMovement()->MovementMode == MOVE_None || !FMath::IsFinite(DashSpeed) ||
        DashSpeed <= 0.f || !FMath::IsFinite(DashDuration) || DashDuration <= 0.f ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[Dash] Activation rejected Avatar=%s Authority=%d"), *GetNameSafe(Character),
               ActorInfo && ActorInfo->IsNetAuthority());
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    if (!IsActive())
    {
        return;
    }
    /**
     * 本地取当前输入，服务器远程角色取运动方向；静止时向角色前方冲刺。
     * 根运动任务携带方向并参与 CharacterMovement 的网络预测与纠正。
     */
    FVector Direction = Character->GetLastMovementInputVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero())
    {
        Direction = Character->GetVelocity().GetSafeNormal2D();
    }
    if (Direction.IsNearlyZero())
    {
        Direction = Character->GetActorForwardVector().GetSafeNormal2D();
    }
    /**
     * 用可撤销根运动源临时覆盖水平速度，保留重力；结束时把速度限制回正常行走上限。
     * 不改 MaxWalkSpeed，避免能力中途撤回后留下永久加速。
     */
    DashTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
        this, TEXT("BuffDash"), Direction, DashSpeed, DashDuration, false, nullptr,
        ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector,
        Character->GetCharacterMovement()->MaxWalkSpeed, true);
    if (!DashTask)
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[Dash] Root motion task creation failed Avatar=%s"), *GetNameSafe(Character));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    /**
     * 先绑定结束回调再启动任务；正常完成会结束 GA，Buff 撤回则由 GAS 取消 GA 和任务。
     */
    DashTask->OnFinish.AddDynamic(this, &ThisClass::OnDashFinished);
    UE_LOG(LogCCBuff, Log,
           TEXT("[Dash] Start Avatar=%s Direction=%s Speed=%.1f Duration=%.2f Authority=%d Predicted=%d"),
           *GetNameSafe(Character), *Direction.ToCompactString(), DashSpeed, DashDuration,
           ActorInfo && ActorInfo->IsNetAuthority(),
           ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Predicting);
    DashTask->ReadyForActivation();
}

/**
 * 只收尾仍激活的能力；任务完成与外部取消可能相邻发生，重复结束需要安全跳过。
 */
void UCC_Dash::OnDashFinished()
{
    if (IsActive())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UCC_Dash::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                          FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (!IsActive())
    {
        return;
    }
    UE_LOG(LogCCBuff, Log, TEXT("[Dash] End Avatar=%s Cancelled=%d"), *GetNameSafe(GetAvatarActorFromActorInfo()),
           bWasCancelled);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
    DashTask = nullptr;
}
