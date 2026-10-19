#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffBehavior.h"
#include "Data/Buff/CC_BuffRulesComponent.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "Data/Buff/CC_BuffDebug.h"
#include "Character/CC_BaseCharacter.h"
#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/CharacterMovementComponent.h"

void FCC_ReplicatedBuffArray::PostReplicatedReceive(
    const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
    if (Owner)
    {
        Owner->NotifyReplicatedChange();
    }
}

UCC_BuffComponent::UCC_BuffComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    ReplicatedBuffs.Owner = this;
}

/**
 * 兼容 ASC 先初始化和后初始化两种时序：先订阅角色通知，再尝试绑定当前 ASC。
 */
void UCC_BuffComponent::BeginPlay()
{
    Super::BeginPlay();
    if (ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(GetOwner()))
    {
        Character->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::InitializeASC);
        InitializeASC(Character->GetAbilitySystemComponent(), Character->GetAttributeSet());
    }
}

/**
 * 统一检查权威、存活、清理状态和 Avatar 归属，避免结束回调中重新生成 Buff。
 * @return 当前角色是否允许服务器施加新 Buff。
 */
bool UCC_BuffComponent::CanApply() const
{
    const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(GetOwner());
    return !bEnded && !bClearing && Character && Character->HasAuthority() && Character->IsAlive() &&
           !Character->IsActorBeingDestroyed() && BoundASC.IsValid() && BoundASC->GetAvatarActor() == Character;
}

/**
 * 解除旧 ASC 和活动 GE 上登记的委托；更换 ASC、退出关卡时都必须成对解绑。
 */
void UCC_BuffComponent::UnbindASC()
{
    if (UAbilitySystemComponent* ASC = BoundASC.Get())
    {
        ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
        ASC->OnGameplayEffectAppliedDelegateToSelf.RemoveAll(this);
        ASC->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
        ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute())
            .RemoveAll(this);
        for (const TPair<FActiveGameplayEffectHandle, FCC_BuffInstance>& Pair : Instances)
        {
            if (FActiveGameplayEffectEvents* Events = ASC->GetActiveEffectEventSet(Pair.Key))
            {
                Events->OnStackChanged.RemoveAll(this);
                Events->OnTimeChanged.RemoveAll(this);
                Events->OnInhibitionChanged.RemoveAll(this);
            }
        }
    }
}

/**
 * 先结束旧实例，再解绑旧 ASC，最后绑定新 ASC；重复绑定同一 ASC 不重复订阅。
 * @param ASC 要接管的新 ASC。
 * @param Attributes 角色初始化广播的属性集参数，当前实现不缓存。
 */
void UCC_BuffComponent::InitializeASC(UAbilitySystemComponent* ASC, UAttributeSet* Attributes)
{
    if (bEnded || BoundASC.Get() == ASC)
    {
        return;
    }
    ClearBuffs(ECC_BuffEndReason::ASCReplaced);
    UnbindASC();
    BoundASC = ASC;
    if (!ASC)
    {
        return;
    }
    if (BaseWalkSpeed <= 0.f)
    {
        if (ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(GetOwner()))
        {
            BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
        }
    }
    ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute())
        .AddUObject(this, &ThisClass::UpdateMovementSpeed);
    UpdateMovementSpeed(FOnAttributeChangeData());
    if (!GetOwner()->HasAuthority())
    {
        return;
    }
    ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
    ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
    ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleEffectRemoved);
    const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
    for (FActiveGameplayEffectHandle Handle : Handles)
    {
        if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle))
        {
            const FGameplayEffectSpec Spec = Effect->Spec;
            HandleEffectAdded(ASC, Spec, Handle);
        }
    }
}

FActiveGameplayEffectHandle UCC_BuffComponent::ApplyBuff(TSubclassOf<UGameplayEffect> EffectClass,
                                                         UAbilitySystemComponent* SourceASC, float Level)
{
    return ApplyBuffWithParameters(EffectClass, SourceASC, Level, {});
}

/**
 * 把配置和本次参数交给 GAS；组件不自行叠加数值，也不自行计算持续时间。
 * @param EffectClass 包含 UI 或 Rules 的 GE 类。
 * @param SourceASC 来源 ASC，未指定时使用持有者。
 * @param Level 正数 GE 等级。
 * @param SetByCaller 本次 Spec 的参数副本。
 * @return GAS 应用结果；失败不会登记新的运行时实例。
 */
FActiveGameplayEffectHandle UCC_BuffComponent::ApplyBuffWithParameters(TSubclassOf<UGameplayEffect> EffectClass,
                                                                       UAbilitySystemComponent* SourceASC, float Level,
                                                                       const TMap<FGameplayTag, float>& SetByCaller)
{
    if (!CanApply() || !EffectClass || EffectClass->HasAnyClassFlags(CLASS_Abstract) || !FMath::IsFinite(Level) ||
        Level <= 0.f)
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[Apply] Rejected Owner=%s Effect=%s Level=%.2f CanApply=%d"),
               *GetNameSafe(GetOwner()), *GetNameSafe(EffectClass.Get()), Level, CanApply());
        return {};
    }

    const UGameplayEffect* Definition = EffectClass.GetDefaultObject();
    const UCC_BuffRulesComponent* Rules = Definition->FindComponent<UCC_BuffRulesComponent>();

    if (!Rules && !Definition->FindComponent<UCC_BuffUIData>())
    {
        return {};
    }

    if (Rules)
    {
        if (Definition->DurationPolicy == EGameplayEffectDurationType::Instant && !Rules->Behaviors.IsEmpty())
        {
            return {};
        }
        for (const UCC_BuffBehavior* Behavior : Rules->Behaviors)
        {
            if (!Behavior || Behavior->GetClass()->HasAnyClassFlags(CLASS_Abstract))
            {
                return {};
            }
        }
    }

    for (const TPair<FGameplayTag, float>& Pair : SetByCaller)
    {
        if (!Pair.Key.IsValid() || !FMath::IsFinite(Pair.Value))
        {
            return {};
        }
    }

    if (!SourceASC)
    {
        SourceASC = BoundASC.Get();
    }
    if (!IsValid(SourceASC) || !SourceASC->IsOwnerActorAuthoritative())
    {
        return {};
    }
    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(Definition);
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(EffectClass, Level, Context);
    if (!Spec.IsValid())
    {
        return {};
    }
    for (const TPair<FGameplayTag, float>& Pair : SetByCaller)
    {
        Spec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
    }

    FActiveGameplayEffectHandle Handle = SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, BoundASC.Get());

    UE_LOG(LogCCBuff, Log, TEXT("[Apply] Owner=%s Effect=%s Level=%.2f Handle=%s Accepted=%d"),
           *GetNameSafe(GetOwner()), *GetNameSafe(EffectClass.Get()), Level, *Handle.ToString(),
           Handle.WasSuccessfullyApplied());

    return Handle;
}

/**
 * 两个 GAS 施加通知可能指向同一句柄；先按 Handle 去重，保证只复制一组 Behavior。
 * @param Source GAS 回调传入的来源 ASC；实际来源快照从 GE Context 读取。
 * @param Spec 当前效果配置和参数。
 * @param Handle 已加入 GAS 的活动效果句柄。
 */
void UCC_BuffComponent::HandleEffectAdded(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec,
                                          FActiveGameplayEffectHandle Handle)
{
    if (bEnded || !GetOwner()->HasAuthority() || !BoundASC.IsValid() || !Spec.Def ||
        !BoundASC->GetActiveGameplayEffect(Handle))
    {
        return;
    }
    const UCC_BuffRulesComponent* Rules = Spec.Def->FindComponent<UCC_BuffRulesComponent>();
    if (!Rules && !Spec.Def->FindComponent<UCC_BuffUIData>())
    {
        return;
    }
    if (!Instances.Contains(Handle))
    {
        FCC_BuffInstance Instance;
        Instance.View.Handle = Handle;
        Instance.View.InstanceId = NextInstanceId++;
        Instance.bRemoveOnDeath = !Rules || Rules->bRemoveOnDeath;
        if (Rules)
        {
            for (UCC_BuffBehavior* Template : Rules->Behaviors)
            {
                if (Template && !Template->GetClass()->HasAnyClassFlags(CLASS_Abstract))
                {
                    Instance.Behaviors.Add(DuplicateObject<UCC_BuffBehavior>(
                        Template, this, MakeUniqueObjectName(this, Template->GetClass(), Template->GetFName())));
                }
            }
        }
        Instances.Add(Handle, MoveTemp(Instance));
        if (FActiveGameplayEffectEvents* Events = BoundASC->GetActiveEffectEventSet(Handle))
        {
            Events->OnStackChanged.AddUObject(this, &ThisClass::HandleStackChanged);
            Events->OnTimeChanged.AddUObject(this, &ThisClass::HandleTimeChanged);
            Events->OnInhibitionChanged.AddUObject(this, &ThisClass::HandleInhibitionChanged);
        }
    }
    RefreshFromGAS(Handle);
}

/**
 * 从 GAS 更新层数、抑制和 UI，所有 Behavior 生命周期仍由这一入口统一驱动。
 * @param Handle 当前活动 GE；已移除的句柄直接跳过。
 * @param bRefreshed 是否需要通知持续时间刷新。
 */
void UCC_BuffComponent::RefreshFromGAS(FActiveGameplayEffectHandle Handle, bool bRefreshed)
{
    FCC_BuffInstance* Instance = Instances.Find(Handle);
    const FActiveGameplayEffect* Effect = BoundASC.IsValid() ? BoundASC->GetActiveGameplayEffect(Handle) : nullptr;
    if (!Instance || !Effect)
    {
        return;
    }
    FCC_BuffInstanceView View = Instance->View;
    View.Owner = GetOwner();
    View.TargetASC = BoundASC;
    View.SourceASC = Effect->Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
    View.Instigator = Effect->Spec.GetContext().GetOriginalInstigator();
    View.StackCount = Effect->Spec.GetStackCount();
    View.Level = Effect->Spec.GetLevel();
    View.SetByCaller = Effect->Spec.SetByCallerTagMagnitudes;
    FCC_ReplicatedBuffItem Item;
    Item.InstanceId = View.InstanceId;
    Item.EffectClass = Effect->Spec.Def->GetClass();
    Effect->Spec.GetAllAssetTags(Item.BuffTags);
    Effect->Spec.GetAllGrantedTags(Item.BuffTags);
    Item.StackCount = View.StackCount;
    Item.Level = View.Level;
    Item.Duration = Effect->GetDuration();
    Item.bIsInhibited = Effect->bIsInhibited;
    Item.EndServerTime =
        Item.Duration < 0.f
            ? -1.f
            : ServerTime() + FMath::Max(0.f, Effect->StartWorldTime + Item.Duration - GetWorld()->GetTimeSeconds());
    UE_LOG(LogCCBuff, Verbose, TEXT("[Refresh] Owner=%s Instance=%d Effect=%s Stacks=%d Inhibited=%d Refreshed=%d"),
           *GetNameSafe(GetOwner()), View.InstanceId, *GetNameSafe(Item.EffectClass.Get()), View.StackCount,
           Item.bIsInhibited, bRefreshed);
    Instance->View = View;
    const bool bEnable = !Item.bIsInhibited && !Instance->bSuspendedForDeath && !bEnded;
    TArray<TStrongObjectPtr<UCC_BuffBehavior>> Behaviors;
    for (UCC_BuffBehavior* Behavior : Instance->Behaviors)
    {
        Behaviors.Emplace(Behavior);
    }
    if (!bEnable)
    {
        SetHooks(Handle, false);
    }
    for (const TStrongObjectPtr<UCC_BuffBehavior>& Pin : Behaviors)
    {
        UCC_BuffBehavior* Behavior = Pin.Get();
        if (!Instances.Contains(Handle))
        {
            return;
        }
        if (Behavior->State == ECC_BuffBehaviorState::Uninitialized)
        {
            Behavior->Initialize(this, View);
        }
        else
        {
            if (!bEnable)
            {
                Behavior->Disable(ECC_BuffDisableReason::Inhibited);
            }
            if (!Instances.Contains(Handle))
            {
                return;
            }
            Behavior->UpdateView(View, bRefreshed);
        }
        if (!Instances.Contains(Handle))
        {
            return;
        }
        if (bEnable)
        {
            Behavior->Enable();
        }
    }
    Instance = Instances.Find(Handle);
    Effect = BoundASC.IsValid() ? BoundASC->GetActiveGameplayEffect(Handle) : nullptr;
    if (!Instance || !Effect)
    {
        return;
    }
    /**
	 * 回调可能同步叠层、抑制或移除 GE；若状态已变化，保留内层刷新结果，不用旧快照覆盖。
	 */
    if (Effect->Spec.GetStackCount() != View.StackCount || Effect->bIsInhibited != Item.bIsInhibited)
    {
        return;
    }
    SetHooks(Handle, bEnable);
    FCC_ReplicatedBuffItem* Existing = ReplicatedBuffs.Items.FindByPredicate(
        [&](const FCC_ReplicatedBuffItem& Entry)
        {
            return Entry.InstanceId == Item.InstanceId;
        });
    if (!Existing)
    {
        Existing = &ReplicatedBuffs.Items.AddDefaulted_GetRef();
    }
    /**
	 * 只逐字段更新展示数据，保留 FastArray 条目的复制身份，避免被误识别成新的 Buff。
	 */
    Existing->InstanceId = Item.InstanceId;
    Existing->EffectClass = Item.EffectClass;
    Existing->BuffTags = Item.BuffTags;
    Existing->StackCount = Item.StackCount;
    Existing->Level = Item.Level;
    Existing->bIsInhibited = Item.bIsInhibited;
    Existing->EndServerTime = Item.EndServerTime;
    Existing->Duration = Item.Duration;
    ReplicatedBuffs.MarkItemDirty(*Existing);
    NotifyChanged();
}

/**
 * 先移除该句柄的旧索引，再按当前 Enabled 实例重建，避免恢复或叠层产生重复调用。
 * @param Handle 要重建索引的 GE。
 * @param bEnabled 是否重新登记规则；为 false 时只摘除索引。
 */
void UCC_BuffComponent::SetHooks(FActiveGameplayEffectHandle Handle, bool bEnabled)
{
    for (TPair<FGameplayTag, TArray<FHookEntry>>& Pair : HookIndex)
    {
        Pair.Value.RemoveAll(
            [Handle](const FHookEntry& Entry)
            {
                return Entry.Handle == Handle;
            });
    }
    const FCC_BuffInstance* Instance = Instances.Find(Handle);
    if (!Instance || !bEnabled)
    {
        return;
    }
    for (int32 Index = 0; Index < Instance->Behaviors.Num(); ++Index)
    {
        UCC_BuffBehavior* Behavior = Instance->Behaviors[Index];
        if (!Behavior->IsEnabled())
        {
            continue;
        }
        for (FGameplayTag Tag : Behavior->HandledHooks)
        {
            TArray<FHookEntry>& Entries = HookIndex.FindOrAdd(Tag);
            Entries.Add({Behavior, Handle, Behavior->HookPriority, Instance->View.InstanceId, Index});
            Entries.Sort(
                [](const FHookEntry& A, const FHookEntry& B)
                {
                    if (A.Priority != B.Priority)
                    {
                        return A.Priority < B.Priority;
                    }
                    if (A.InstanceId != B.InstanceId)
                    {
                        return A.InstanceId < B.InstanceId;
                    }
                    return A.BehaviorIndex < B.BehaviorIndex;
                });
        }
    }
}

/**
 * 对活动规则执行本次钩子；快照索引确保回调中移除 Buff 也不会破坏遍历。
 * @param Hook 当前决策点的标签。
 * @param Context 只允许修改字段，不允许替换结构体类型。
 */
void UCC_BuffComponent::RunHook(FGameplayTag Hook, FInstancedStruct& Context)
{
    if (!GetOwner()->HasAuthority() || bEnded || bClearing || !Context.IsValid() || HookDepth >= 8)
    {
        return;
    }
    const TArray<FHookEntry>* Entries = HookIndex.Find(Hook);
    if (!Entries)
    {
        return;
    }

    /**
	 * 本次调用只使用开始时的索引；回调新增的规则留到下一次钩子。
	 */
    const TArray<FHookEntry> Snapshot = *Entries;
    UE_LOG(LogCCBuff, Verbose, TEXT("[Hook] Owner=%s Tag=%s Rules=%d Depth=%d"), *GetNameSafe(GetOwner()),
           *Hook.ToString(), Snapshot.Num(), HookDepth);
    const UScriptStruct* ExpectedType = Context.GetScriptStruct();
    TGuardValue<int32> DepthGuard(HookDepth, HookDepth + 1);
    for (const FHookEntry& Entry : Snapshot)
    {
        TStrongObjectPtr<UCC_BuffBehavior> Behavior(Entry.Behavior.Get());
        if (!Behavior.IsValid() || !Behavior->IsEnabled() || !Instances.Contains(Entry.Handle))
        {
            continue;
        }
        const FInstancedStruct Before = Context;
        Behavior->ModifyHook(Hook, Context);
        if (Context.GetScriptStruct() != ExpectedType)
        {
            /**
			 * 恢复到当前规则执行前的值，保留之前其他规则的修改，防止错误类型传给发射系统。
			 */
            Context = Before;
            ensureMsgf(false, TEXT("Buff hook replaced its context type."));
        }
    }
}

bool UCC_BuffComponent::RemoveBuff(int32 InstanceId)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid())
    {
        return false;
    }
    for (const TPair<FActiveGameplayEffectHandle, FCC_BuffInstance>& Pair : Instances)
    {
        if (Pair.Value.View.InstanceId == InstanceId)
        {
            const FActiveGameplayEffectHandle Handle = Pair.Key;
            return BoundASC->RemoveActiveGameplayEffect(Handle, -1);
        }
    }
    return false;
}

bool UCC_BuffComponent::ConsumeStacks(FActiveGameplayEffectHandle Handle, int32 Count)
{
    return GetOwner()->HasAuthority() && BoundASC.IsValid() && Count > 0 && Instances.Contains(Handle) &&
           BoundASC->RemoveActiveGameplayEffect(Handle, Count);
}

int32 UCC_BuffComponent::RemoveBuffsByQuery(const FGameplayTagQuery& Query, int32 MaxCount)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid() || MaxCount == 0 || MaxCount < -1)
    {
        return 0;
    }
    TArray<int32> Ids;
    for (const FCC_ReplicatedBuffItem& Item : ReplicatedBuffs.Items)
    {
        if (Query.IsEmpty() || Query.Matches(Item.BuffTags))
        {
            Ids.Add(Item.InstanceId);
        }
    }
    Ids.Sort(); // Deterministic oldest-first removal.
    int32 Removed = 0;
    for (int32 Id : Ids)
    {
        if ((MaxCount < 0 || Removed < MaxCount) && RemoveBuff(Id))
        {
            ++Removed;
        }
    }
    return Removed;
}

int32 UCC_BuffComponent::GetStackCountByTag(FGameplayTag Tag, bool bIncludeInhibited) const
{
    int32 Count = 0;
    for (const FCC_ReplicatedBuffItem& Item : ReplicatedBuffs.Items)
    {
        if (Item.BuffTags.HasTag(Tag) && (bIncludeInhibited || !Item.bIsInhibited))
        {
            Count += Item.StackCount;
        }
    }
    return Count;
}

void UCC_BuffComponent::HandleEffectRemoved(const FActiveGameplayEffect& Effect)
{
    const ECC_BuffEndReason* Reason = RemovalReasons.Find(Effect.Handle);
    const ECC_BuffEndReason EndReason = Reason ? *Reason : ECC_BuffEndReason::EffectRemoved;
    RemovalReasons.Remove(Effect.Handle);
    FinishInstance(Effect.Handle, EndReason);
}

/**
 * 先移除索引和记录，再对称回收 Behavior，避免结束回调重入再次执行同一规则。
 * @param Handle 要结束的 GE 句柄。
 * @param Reason 到期、驱散、死亡或关卡结束等原因。
 */
void UCC_BuffComponent::FinishInstance(FActiveGameplayEffectHandle Handle, ECC_BuffEndReason Reason)
{
    FCC_BuffInstance Instance;
    if (!Instances.RemoveAndCopyValue(Handle, Instance))
    {
        return;
    }
    UE_LOG(LogCCBuff, Log, TEXT("[Remove] Owner=%s Instance=%d Handle=%s Reason=%d Behaviors=%d"),
           *GetNameSafe(GetOwner()), Instance.View.InstanceId, *Handle.ToString(), static_cast<int32>(Reason),
           Instance.Behaviors.Num());
    SetHooks(Handle, false);
    if (BoundASC.IsValid())
    {
        if (FActiveGameplayEffectEvents* Events = BoundASC->GetActiveEffectEventSet(Handle))
        {
            Events->OnStackChanged.RemoveAll(this);
            Events->OnTimeChanged.RemoveAll(this);
            Events->OnInhibitionChanged.RemoveAll(this);
        }
    }
    ReplicatedBuffs.Items.RemoveAll(
        [&](const FCC_ReplicatedBuffItem& Item)
        {
            return Item.InstanceId == Instance.View.InstanceId;
        });
    ReplicatedBuffs.MarkArrayDirty();
    TArray<TStrongObjectPtr<UCC_BuffBehavior>> Behaviors;
    for (UCC_BuffBehavior* Behavior : Instance.Behaviors)
    {
        Behaviors.Emplace(Behavior);
    }
    for (const TStrongObjectPtr<UCC_BuffBehavior>& Pin : Behaviors)
    {
        Pin->Shutdown(Reason);
    }
    NotifyChanged();
}

/**
 * 快照句柄列表后清理，允许移除 GE 的同步回调修改 Instances。
 * @param Reason 决定完整结束，或在持有者死亡时仅暂停配置为保留的 GE。
 */
void UCC_BuffComponent::ClearBuffs(ECC_BuffEndReason Reason)
{
    if (!GetOwner()->HasAuthority() || bClearing)
    {
        return;
    }
    TGuardValue<bool> Guard(bClearing, true);
    TArray<FActiveGameplayEffectHandle> Handles;
    Instances.GetKeys(Handles);
    for (FActiveGameplayEffectHandle Handle : Handles)
    {
        FCC_BuffInstance* Instance = Instances.Find(Handle);
        if (!Instance)
        {
            continue;
        }
        if (Reason == ECC_BuffEndReason::OwnerDeath && !Instance->bRemoveOnDeath)
        {
            Instance->bSuspendedForDeath = true;
            const TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors = Instance->Behaviors;
            SetHooks(Handle, false);
            for (UCC_BuffBehavior* Behavior : Behaviors)
            {
                TStrongObjectPtr<UCC_BuffBehavior> Pin(Behavior);
                Behavior->Disable(ECC_BuffDisableReason::OwnerDeath);
            }
            continue;
        }
        RemovalReasons.Add(Handle, Reason);
        if (BoundASC.IsValid())
        {
            BoundASC->RemoveActiveGameplayEffect(Handle, -1);
        }
        RemovalReasons.Remove(Handle);
        FinishInstance(Handle, Reason); /** GAS 未回调或 ASC 已解绑时也走幂等收尾。 */
    }
}

void UCC_BuffComponent::HandleStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount)
{
    UE_LOG(LogCCBuff, Log, TEXT("[Stack] Owner=%s Handle=%s Stacks=%d->%d"), *GetNameSafe(GetOwner()),
           *Handle.ToString(), OldCount, NewCount);
    RefreshFromGAS(Handle);
}

void UCC_BuffComponent::HandleTimeChanged(FActiveGameplayEffectHandle Handle, float Start, float Duration)
{
    RefreshFromGAS(Handle, true);
}

void UCC_BuffComponent::HandleInhibitionChanged(FActiveGameplayEffectHandle Handle, bool bInhibited)
{
    UE_LOG(LogCCBuff, Log, TEXT("[Inhibition] Owner=%s Handle=%s Inhibited=%d"), *GetNameSafe(GetOwner()),
           *Handle.ToString(), bInhibited);
    RefreshFromGAS(Handle);
}

/**
 * 客户端优先使用 GameState 校准后的服务器时间；没有 GameState 时回退到世界时间。
 * @return 用于比较展示到期时刻的秒数。
 */
float UCC_BuffComponent::ServerTime() const
{
    const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

/**
 * 既有移速适配只读取通用 GAS 属性，不识别疾行等具体 Buff，移除后自然恢复。
 * @param Data 属性变化通知；最终速度始终用当前聚合属性重新计算。
 */
void UCC_BuffComponent::UpdateMovementSpeed(const FOnAttributeChangeData& Data)
{
    if (ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(GetOwner()))
    {
        if (BoundASC.IsValid())
        {
            Character->GetCharacterMovement()->MaxWalkSpeed =
                BaseWalkSpeed * BoundASC->GetNumericAttribute(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute());
        }
    }
}

float UCC_BuffComponent::GetRemainingTime(int32 InstanceId) const
{
    const FCC_ReplicatedBuffItem* Item = ReplicatedBuffs.Items.FindByPredicate(
        [InstanceId](const FCC_ReplicatedBuffItem& Entry)
        {
            return Entry.InstanceId == InstanceId;
        });
    return !Item ? 0.f : (Item->EndServerTime < 0.f ? -1.f : FMath::Max(0.f, Item->EndServerTime - ServerTime()));
}

void UCC_BuffComponent::NotifyChanged()
{
    if (!bEnded)
    {
        GetOwner()->ForceNetUpdate();
        OnBuffsChanged.Broadcast();
    }
}

void UCC_BuffComponent::NotifyReplicatedChange()
{
    OnBuffsChanged.Broadcast();
}

void UCC_BuffComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, ReplicatedBuffs);
}

/**
 * 先标记结束以阻止新施加和 UI 广播，再清理效果、规则资源及所有 ASC 订阅。
 * @param Reason 引擎提供的组件退出原因。
 */
void UCC_BuffComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnded = true;
    ClearBuffs(ECC_BuffEndReason::EndPlay);
    UnbindASC();
    BoundASC.Reset();
    if (ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(GetOwner()))
    {
        Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::InitializeASC);
    }
    Super::EndPlay(Reason);
}
