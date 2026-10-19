#include "Data/Buff/CC_BuffBehavior.h"
#include "Data/Buff/CC_BuffDebug.h"
#include "Components/CC_BuffComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UWorld* UCC_BuffBehavior::GetWorld() const
{
    /**
     * 模板副本可能保留 Archetype 标志；用运行时组件绑定判断世界，不能仅凭该标志排除实例。
     */
    return !HasAnyFlags(RF_ClassDefaultObject) && Component.IsValid() ? Component->GetWorld() : nullptr;
}

/**
 * 先建立组件与快照绑定，再初始化一次；首次被抑制的 GE 也会创建 Disabled 实例。
 * @param InComponent 拥有运行时实例的 Buff 组件。
 * @param InView 当前 GE 的按值快照。
 */
void UCC_BuffBehavior::Initialize(UCC_BuffComponent* InComponent, const FCC_BuffInstanceView& InView)
{
    check(State == ECC_BuffBehaviorState::Uninitialized);
    Component = InComponent;
    View = InView;
    State = ECC_BuffBehaviorState::Disabled;
    OnInitialized();
}

/**
 * 先保存新快照再调用子类，确保回调读取 GetContext 时拿到新层数。
 * @param InView 当前 GAS 状态的副本。
 * @param bRefreshed 是否额外发送持续时间刷新通知。
 */
void UCC_BuffBehavior::UpdateView(const FCC_BuffInstanceView& InView, bool bRefreshed)
{
    if (State == ECC_BuffBehaviorState::Finished)
    {
        return;
    }
    const int32 OldStacks = View.StackCount;
    View = InView;
    if (OldStacks != View.StackCount)
    {
        OnStackChanged(OldStacks, View.StackCount);
    }
    if (bRefreshed && State != ECC_BuffBehaviorState::Finished)
    {
        OnRefreshed();
    }
}

/**
 * 先进入 Enabled 再调用子类，使 OnEnabled 能使用资源登记工具；重复启用直接忽略。
 */
void UCC_BuffBehavior::Enable()
{
    if (State != ECC_BuffBehaviorState::Disabled)
    {
        return;
    }
    State = ECC_BuffBehaviorState::Enabled;
    UE_LOG(LogCCBuff, Log, TEXT("[Behavior][Enable] Owner=%s Instance=%d Behavior=%s Stacks=%d"),
           *GetNameSafe(View.Owner.Get()), View.InstanceId, *GetNameSafe(this), View.StackCount);
    OnEnabled();
}

/**
 * 先停止接受钩子和事件，再通知子类并释放启用期间资源；保留实例便于恢复。
 * @param Reason 抑制、死亡或最终结束等原因。
 */
void UCC_BuffBehavior::Disable(ECC_BuffDisableReason Reason)
{
    if (!IsEnabled())
    {
        return;
    }
    State = ECC_BuffBehaviorState::Disabled;
    UE_LOG(LogCCBuff, Log, TEXT("[Behavior][Disable] Owner=%s Instance=%d Behavior=%s Reason=%d"),
           *GetNameSafe(View.Owner.Get()), View.InstanceId, *GetNameSafe(this), static_cast<int32>(Reason));
    OnDisabled(Reason);
    ReleaseResources(false);
}

/**
 * 先置 Finished 防止结束回调重入；最终结束只执行一次，并回收所有受管资源。
 * @param Reason 父组件传入的最终结束原因。
 */
void UCC_BuffBehavior::Shutdown(ECC_BuffEndReason Reason)
{
    if (State == ECC_BuffBehaviorState::Finished)
    {
        return;
    }
    const bool bWasEnabled = IsEnabled();
    State = ECC_BuffBehaviorState::Finished; // Teardown can re-enter through child effects.
    UE_LOG(LogCCBuff, Log, TEXT("[Behavior][Shutdown] Owner=%s Instance=%d Behavior=%s Reason=%d"),
           *GetNameSafe(View.Owner.Get()), View.InstanceId, *GetNameSafe(this), static_cast<int32>(Reason));
    if (bWasEnabled)
    {
        OnDisabled(ECC_BuffDisableReason::Ending);
    }
    OnShutdown(Reason);
    ReleaseResources(true);
    Component.Reset();
}

void UCC_BuffBehavior::ListenGameplayEvent(FGameplayTag Tag)
{
    UAbilitySystemComponent* ASC = View.TargetASC.Get();
    if (!IsEnabled() || !ASC || !Tag.IsValid())
    {
        return;
    }
    if (Subscriptions.ContainsByPredicate(
            [Tag](const FEventSubscription& Entry)
            {
                return Entry.Tags.HasTagExact(Tag);
            }))
    {
        return;
    }
    FGameplayTagContainer Tags(Tag);
    FDelegateHandle Handle = ASC->AddGameplayEventTagContainerDelegate(
        Tags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::ReceiveGameplayEvent));
    Subscriptions.Add({ASC, Tags, Handle});
}

void UCC_BuffBehavior::ReceiveGameplayEvent(FGameplayTag Tag, const FGameplayEventData* Event)
{
    if (IsEnabled() && Event)
    {
        /**
         * 事件回调可能同步触发嵌套事件；离开当前作用域后恢复外层 Context，
         * 避免后续子效果继承到错误的来源信息。
         */
        TGuardValue<FGameplayEffectContextHandle> Guard(CurrentEventContext, Event->ContextHandle);
        OnGameplayEvent(Tag, *Event);
    }
}

FTimerHandle UCC_BuffBehavior::StartTimer(float Interval, bool bLoop, const FTimerDelegate& Callback)
{
    FTimerHandle Handle;
    if (!IsEnabled() || !GetWorld() || !FMath::IsFinite(Interval) || Interval <= 0.f)
    {
        return Handle;
    }
    GetWorld()->GetTimerManager().SetTimer(Handle, Callback, Interval, bLoop);
    Timers.Add(Handle);
    return Handle;
}

AActor* UCC_BuffBehavior::SpawnOwnedActor(TSubclassOf<AActor> Class, const FTransform& Transform,
                                          ECC_BuffResourceLifetime Lifetime)
{
    if (!IsEnabled() || !Class || !GetWorld())
    {
        return nullptr;
    }
    FActorSpawnParameters Params;
    Params.Owner = View.Owner.Get();
    Params.Instigator = Cast<APawn>(View.Owner.Get());
    AActor* Actor = GetWorld()->SpawnActor<AActor>(Class, Transform, Params);
    if (Actor && Lifetime != ECC_BuffResourceLifetime::Independent)
    {
        if (IsEnabled())
        {
            Actors.Add({Actor, Lifetime});
        }
        else
        {
            Actor->Destroy(); // BeginPlay may have removed the parent synchronously.
        }
    }
    return Actor;
}

FActiveGameplayEffectHandle UCC_BuffBehavior::ApplyChildEffect(UAbilitySystemComponent* Target,
                                                               TSubclassOf<UGameplayEffect> Effect, float Level,
                                                               ECC_BuffResourceLifetime Lifetime)
{
    UAbilitySystemComponent* Source = View.SourceASC.Get();
    if (!Source)
    {
        Source = View.TargetASC.Get();
    }
    if (!IsEnabled() || !IsValid(Source) || !IsValid(Target) || !Effect || !Source->IsOwnerActorAuthoritative() ||
        !Target->IsOwnerActorAuthoritative() || !FMath::IsFinite(Level) || Level <= 0.f)
    {
        return {};
    }
    /**
     * 堆叠 GE 的句柄可能被多个来源共享，不能由某个父 Buff 单独负责撤回。
     */
    if (Lifetime != ECC_BuffResourceLifetime::Independent &&
        Effect.GetDefaultObject()->StackingType != EGameplayEffectStackingType::None)
    {
        return {};
    }
    FGameplayEffectContextHandle Context =
        CurrentEventContext.IsValid() ? CurrentEventContext.Duplicate() : Source->MakeEffectContext();
    FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Effect, Level, Context);
    if (!Spec.IsValid())
    {
        return {};
    }
    FActiveGameplayEffectHandle Handle = Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
    if (Handle.IsValid() && Lifetime != ECC_BuffResourceLifetime::Independent)
    {
        if (IsEnabled())
        {
            Effects.Add({Target, Handle, Lifetime});
        }
        else
        {
            Target->RemoveActiveGameplayEffect(Handle);
        }
    }
    return Handle;
}

bool UCC_BuffBehavior::ConsumeStacks(int32 Count)
{
    return IsEnabled() && Component.IsValid() && Component->ConsumeStacks(View.Handle, Count);
}

/**
 * 先移走订阅与计时器，并摘下待释放的 Actor/GE 记录，再触发外部销毁和移除回调。
 * @param bFinal 是否连同 Instance 资源一起释放；Independent 资源从未登记在此。
 */
void UCC_BuffBehavior::ReleaseResources(bool bFinal)
{
    TArray<FEventSubscription> OldSubscriptions = MoveTemp(Subscriptions);
    TArray<FTimerHandle> OldTimers = MoveTemp(Timers);
    for (const FEventSubscription& Entry : OldSubscriptions)
    {
        if (UAbilitySystemComponent* ASC = Entry.ASC.Get())
        {
            ASC->RemoveGameplayEventTagContainerDelegate(Entry.Tags, Entry.Handle);
        }
    }
    if (GetWorld())
    {
        for (FTimerHandle& Handle : OldTimers)
        {
            GetWorld()->GetTimerManager().ClearTimer(Handle);
        }
    }
    TArray<FOwnedActor> ActorsToRelease;
    TArray<FChildEffect> EffectsToRelease;
    /**
     * 先提取再释放，避免销毁 Actor 或移除子 GE 的同步回调重复处理同一记录。
     */
    for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
    {
        if (bFinal || Actors[Index].Lifetime == ECC_BuffResourceLifetime::WhileEnabled)
        {
            ActorsToRelease.Add(Actors[Index]);
            Actors.RemoveAt(Index);
        }
    }
    for (int32 Index = Effects.Num() - 1; Index >= 0; --Index)
    {
        if (bFinal || Effects[Index].Lifetime == ECC_BuffResourceLifetime::WhileEnabled)
        {
            EffectsToRelease.Add(Effects[Index]);
            Effects.RemoveAt(Index);
        }
    }
    for (const FOwnedActor& Entry : ActorsToRelease)
    {
        if (AActor* Actor = Entry.Actor.Get())
        {
            Actor->Destroy();
        }
    }
    for (const FChildEffect& Entry : EffectsToRelease)
    {
        if (UAbilitySystemComponent* ASC = Entry.ASC.Get())
        {
            ASC->RemoveActiveGameplayEffect(Entry.Handle);
        }
    }
}
