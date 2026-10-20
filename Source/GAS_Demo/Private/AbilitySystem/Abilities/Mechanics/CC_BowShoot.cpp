#include "AbilitySystem/Abilities/Mechanics/CC_BowShoot.h"

#include "AbilitySystemComponent.h"

#include "Actors/Character/CC_EnemyCharacter.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Components/ActionComponent.h"
#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Data/Buff/CC_BuffDebug.h"
#include "Actors/Projectiles/CC_ProjectileFireContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/AbilityData/CombatActionData.h"
#include "Data/CC_CharacterConfig.h"
#include "GameplayTags/CC_Tags.h"
#include "Engine/World.h"
#include "GAS_Demo.h"

UCC_BowShoot::UCC_BowShoot()
{
    // 每次射击使用独立能力实例，放箭状态不会被其他弓兵共享。
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
    // 只有服务器执行放箭逻辑：服务器生成可复制的箭，并负责最终命中判定。
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    // 一次射击未结束时，不允许相同能力重新激活并重置正在使用的状态。
    bRetriggerInstancedAbility = true;
    // 资产标签用于按 Tag 查找已授予的能力；它本身不等于“已经激活”。
    SetAssetTags(FGameplayTagContainer(CCTags::CCAbilityTrigger::BowShoot));
    // 激活期间给 ASC 挂上 Attack 状态：KeepDistance 据此暂缓后撤；AI 控制器据此切换 Crowd ObstacleOnly。
    // 弓兵不持有令牌，标签 1→0 时控制器的归还请求会被围攻子系统忽略。
    ActivationOwnedTags.AddTag(CCTags::CCAbilities::Attack);

    // 收到 BowShoot 游戏事件时自动激活本能力，事件携带本次要播放的动作数据。
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag = CCTags::CCAbilityTrigger::BowShoot;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(Trigger);
}

void UCC_BowShoot::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                   const FGameplayAbilityActivationInfo ActivationInfo,
                                   const FGameplayEventData* TriggerEventData)
{
    // 父类负责 GAS 激活流程，并从 TriggerEventData->OptionalObject 解析 OwnedAction。
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    // 父类校验可能已结束能力；此时不能继续读取角色或播放动画。
    if (!IsActive())
    {
        UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Shoot] Activation stopped by base ability. Avatar=%s"),
               *GetNameSafe(GetAvatarActorFromActorInfo()));
        return;
    }

    // Avatar 是真正施放能力的弓兵；Config 是共享配置，Target 是索敌时保存的弱引用。
    ACC_EnemyCharacter* Enemy = Cast<ACC_EnemyCharacter>(GetAvatarActorFromActorInfo());
    const UCC_CharacterConfig* Config = IsValid(Enemy) ? Enemy->GetCharacterConfig() : nullptr;
    AActor* Target = IsValid(Enemy) ? Enemy->GetClosestActor().Actor.Get() : nullptr;

    // 非本项目角色也可能成为 Actor 目标；能转换为角色时额外检查是否已死亡。
    const ACC_BaseCharacter* TargetCharacter = Cast<ACC_BaseCharacter>(Target);
    // 缺少服务器权限、角色、箭类、目标或蒙太奇时，不能开始一次有效射击。
    if (!ActorInfo || !ActorInfo->IsNetAuthority() || !IsValid(Enemy) || !Enemy->IsAlive() || !IsValid(Config) ||
        !Config->ArrowProjectileClass || !IsValid(Target) ||
        (IsValid(TargetCharacter) && !TargetCharacter->IsAlive()) || !IsValid(CombatComponent) ||
        !IsValid(OwnedAction) || !IsValid(OwnedAction->Montage))
    {
        UE_LOG(
            LogGAS_Demo, Warning,
            TEXT(
                "[BowAI][Shoot] Invalid activation: Enemy=%s Alive=%d Config=%s ArrowClass=%s Target=%s Combat=%s Action=%s Montage=%s Authority=%d"),
            *GetNameSafe(Enemy), IsValid(Enemy) && Enemy->IsAlive(), *GetNameSafe(Config),
            IsValid(Config) ? *GetNameSafe(Config->ArrowProjectileClass.Get()) : TEXT("None"), *GetNameSafe(Target),
            *GetNameSafe(CombatComponent), *GetNameSafe(OwnedAction),
            IsValid(OwnedAction) ? *GetNameSafe(OwnedAction->Montage) : TEXT("None"),
            ActorInfo && ActorInfo->IsNetAuthority());
        // 最后两个 true 分别表示复制结束状态、将此次结束标记为取消。
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 只算水平距离，避免地形高低差改变弓兵与目标的战斗距离判断。
    const float Distance = FVector::Dist2D(Enemy->GetActorLocation(), Target->GetActorLocation());

    // 太近应由后撤能力处理，太远不能射击；CommitAbility 检查并支付 GAS 成本/冷却。
    if (Distance < Config->MinRange || Distance > Config->MaxRange)
    {
        UE_LOG(LogGAS_Demo, Log,
               TEXT("[BowAI][Shoot] Outside firing range: Enemy=%s Target=%s Distance=%.1f Min=%.1f Max=%.1f"),
               *GetNameSafe(Enemy), *GetNameSafe(Target), Distance, Config->MinRange, Config->MaxRange);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    // if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    // {
    // 	UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Shoot] CommitAbility failed: Enemy=%s Target=%s"),
    // 		*GetNameSafe(Enemy), *GetNameSafe(Target));
    // 	EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    // 	return;
    // }
    // 提交成本可能触发外部取消，因此在创建动画任务前再检查一次。
    if (!IsActive())
    {
        return;
    }

    const FGameplayTag ReleaseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("CCTags.CCAbilityTrigger.BowRelease")));

    //等待蒙太奇发送射箭事件
    ReleaseEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ReleaseTag, nullptr, true, true);
    if (!IsValid(ReleaseEventTask))
    {
        UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Shoot] Could not create release event task: Enemy=%s"),
               *GetNameSafe(Enemy));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 建立等待蒙太奇的 AbilityTask；最后的 true 表示能力结束时停止蒙太奇。
    MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, TEXT("BowShootMontage"), OwnedAction->Montage, 1.0f, NAME_None,
        true, // 能力结束时停止蒙太奇
        1.0f, // Root Motion 缩放
        0.0f, // 播放起始时间
        true  // 混出后再被打断，仍触发 OnInterrupted
    );

    if (!IsValid(MontageTask))
    {
        UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Shoot] Could not create montage task: Enemy=%s Montage=%s"),
               *GetNameSafe(Enemy), *GetNameSafe(OwnedAction->Montage));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 动画任务创建成功后，才把这次动作登记到原有连招组件中。
    CombatComponent->GetAttackIndex(OwnedAction);
    // 本次激活尚未尝试放箭；该标记阻止同一次能力重复生成箭。
    bArrowReleased = false;
    // 完成、混出、打断、取消分别进入父类回调；打断/取消会结束能力。
    MontageTask->OnCompleted.AddDynamic(this, &ThisClass::OnMontageCompleted);
    MontageTask->OnBlendOut.AddDynamic(this, &ThisClass::OnMontageBlendOut);
    MontageTask->OnInterrupted.AddDynamic(this, &ThisClass::OnMontageInterrupted);
    MontageTask->OnCancelled.AddDynamic(this, &ThisClass::OnMontageCancelled);

    ReleaseEventTask->EventReceived.AddDynamic(this, &ThisClass::OnBowRelease);
    UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Shoot] Start: Enemy=%s Target=%s Distance=%.1f Montage=%s ReleaseTag=%s"),
           *GetNameSafe(Enemy), *GetNameSafe(Target), Distance, *GetNameSafe(OwnedAction->Montage),
           *ReleaseTag.ToString());
    // 先绑定回调再启动任务，因为启动时就可能同步失败或结束。
    ReleaseEventTask->ReadyForActivation();
    MontageTask->ReadyForActivation();
}

void UCC_BowShoot::OnBowRelease(FGameplayEventData Payload)
{
    UE_LOG(LogGAS_Demo, Log,
           TEXT("[BowAI][Shoot] Release event received: Enemy=%s Tag=%s Active=%d AlreadyReleased=%d"),
           *GetNameSafe(GetAvatarActorFromActorInfo()), *Payload.EventTag.ToString(), IsActive(), bArrowReleased);
    if (!IsActive() || bArrowReleased)
    {
        return;
    }

    ReleaseArrow();
}

void UCC_BowShoot::ReleaseArrow()
{
    // 动画取消或重复收到事件时，不能再次生成箭。
    if (!IsActive() || bArrowReleased)
    {
        UE_LOG(LogGAS_Demo, Verbose, TEXT("[BowAI][Shoot] Release skipped: Active=%d AlreadyReleased=%d"), IsActive(),
               bArrowReleased);
        return;
    }

    // 标记的是“已尝试放箭”；即使随后目标失效，本次动画也不会再发第二箭。
    bArrowReleased = true;
    // 放箭时重新获取目标，不能依赖动画开始那一刻保存的位置或有效性。
    const UCC_CharacterConfig* Config = IsValid(BaseCharacter) ? BaseCharacter->GetCharacterConfig() : nullptr;
    AActor* Target = IsValid(BaseCharacter) ? BaseCharacter->GetClosestActor().Actor.Get() : nullptr;
    const ACC_BaseCharacter* TargetCharacter = Cast<ACC_BaseCharacter>(Target);

    if (!IsValid(Config) || !IsValid(Target) || !Config->ArrowProjectileClass ||
        (IsValid(TargetCharacter) && !TargetCharacter->IsAlive()))
    {
        UE_LOG(LogGAS_Demo, Warning, TEXT("[BowAI][Shoot] Release aborted: Enemy=%s Config=%s Target=%s ArrowClass=%s"),
               *GetNameSafe(BaseCharacter), *GetNameSafe(Config), *GetNameSafe(Target),
               IsValid(Config) ? *GetNameSafe(Config->ArrowProjectileClass.Get()) : TEXT("None"));
        return;
    }

    // 1/ 动画播放期间目标可能跑近或跑远；离开合法射程就放弃这支箭。
    const float Distance = FVector::Dist2D(BaseCharacter->GetActorLocation(), Target->GetActorLocation());
    if (Distance < Config->MinRange || Distance > Config->MaxRange)
    {
        UE_LOG(LogGAS_Demo, Log,
               TEXT("[BowAI][Shoot] Release aborted by range: Enemy=%s Target=%s Distance=%.1f Min=%.1f Max=%.1f"),
               *GetNameSafe(BaseCharacter), *GetNameSafe(Target), Distance, Config->MinRange, Config->MaxRange);
        return;
    }

    FVector Origin =
        BaseCharacter->GetActorLocation() + BaseCharacter->GetActorForwardVector() * 75.0f + FVector(0.0f, 0.0f, 75.0f);
    FVector Direction = BaseCharacter->GetActorForwardVector();
    FString SpawnSource = TEXT("ActorFallback");

    // 拿到弓箭手的射箭模型
    TArray<UStaticMeshComponent*> MeshComponents;
    BaseCharacter->GetComponents<UStaticMeshComponent>(MeshComponents);

    for (const UStaticMeshComponent* Component : MeshComponents)
    {
        if (IsValid(Component) && Component->GetName().StartsWith(TEXT("Arrow02SM")))
        {
            Origin = Component->GetComponentLocation();
            Direction = -Component->GetComponentTransform().GetUnitAxis(EAxis::X);
            SpawnSource = Component->GetName();
            break;
        }
    }

    /**
	 * Build：只读取射击配置与动作，默认仍然发射一支完整伤害的箭。
	 */
    FCC_ProjectileFireContext Fire;
    Fire.Origin = Origin;
    Fire.AimDirection = Direction;
    Fire.ProjectileClass = Config->ArrowProjectileClass;
    Fire.Speed = Config->ArrowSpeed;
    Fire.Payload.FlatDamage = OwnedAction->Damage;
    Fire.Payload.SourceTags.AddTag(CCBuffTags::ProjectileDamage);

    /**
	 * Modify：系统只调用通用决策点，规则层决定是否修改本次参数。
	 */
    if (UCC_BuffComponent* Buffs = BaseCharacter->GetBuffComponent())
    {
        Buffs->RunHook(CCBuffTags::ProjectileFire, Fire);
    }
    if (!IsActive() || !IsValid(BaseCharacter) || !BaseCharacter->IsAlive() || !GetWorld())
    {
        return;
    }
    /**
	 * Resolve：集中展开对称散射方向，不在 Behavior 中生成箭。
	 */
    TArray<FVector> Directions;
    Fire.Resolve(Directions);

    UE_LOG(LogCCBuff, Log, TEXT("[Fire] Shooter=%s Target=%s Source=%s Count=%d Spread=%.1f Ratio=%.3f"),
           *GetNameSafe(BaseCharacter), *GetNameSafe(Target), *SpawnSource, Directions.Num(), Fire.SpreadAngle,
           Fire.Payload.DamageMultiplier);

    /**
	 * Execute：先准备每支箭的快照与同齐射忽略列表，再完成生成并启动碰撞。
	 * 已经发出的箭独立存在，后续 Buff 移除只影响下一次放箭。
	 */
    TArray<ACC_ArrowProjectile*> Arrows;
    TArray<FTransform> Transforms;
    for (const FVector& ShotDirection : Directions)
    {
        const FTransform Transform(ShotDirection.Rotation(), Fire.Origin);
        ACC_ArrowProjectile* Arrow = GetWorld()->SpawnActorDeferred<ACC_ArrowProjectile>(
            Fire.ProjectileClass, Transform, BaseCharacter, BaseCharacter,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!IsValid(Arrow))
        {
            UE_LOG(LogCCBuff, Warning, TEXT("[Fire] Spawn failed Shooter=%s Class=%s"), *GetNameSafe(BaseCharacter),
                   *GetNameSafe(Fire.ProjectileClass.Get()));
            continue;
        }
        if (!Arrow->InitializeArrow(Fire.Speed, Fire.Payload))
        {
            UE_LOG(LogCCBuff, Warning, TEXT("[Fire] Damage snapshot failed Arrow=%s"), *GetNameSafe(Arrow));
            Arrow->Destroy();
            continue;
        }
        Arrows.Add(Arrow);
        Transforms.Add(Transform);
    }
    for (ACC_ArrowProjectile* Arrow : Arrows)
    {
        for (ACC_ArrowProjectile* Other : Arrows)
        {
            Arrow->IgnoreVolleyArrow(Other);
        }
    }
    for (int32 Index = 0; Index < Arrows.Num(); ++Index)
    {
        if (IsValid(Arrows[Index]))
        {
            Arrows[Index]->FinishSpawning(Transforms[Index]);
        }
    }
}

void UCC_BowShoot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                              const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
                              bool bWasCancelled)
{
    UE_LOG(LogGAS_Demo, Log, TEXT("[BowAI][Shoot] End: Enemy=%s Cancelled=%d Released=%d Action=%s"),
           *GetNameSafe(GetAvatarActorFromActorInfo()), bWasCancelled, bArrowReleased, *GetNameSafe(OwnedAction));

    // 外部打断（死亡、BT 取消）也清理动作锁，避免下一次 TryAttack 被连招状态挡住。
    if (IsValid(CombatComponent))
    {
        CombatComponent->EndActionIfOwned(OwnedAction);
    }
    // 交还 GAS 生命周期管理，移除激活时持有的 Attack 标签并通知等待者。
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
