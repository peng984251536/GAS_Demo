#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffBehavior.h"
#include "Data/Buff/CC_BuffRulesComponent.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "Character/CC_BaseCharacter.h"
#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/CharacterMovementComponent.h"

void FCC_ReplicatedBuffArray::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters)
{
    if (Owner) Owner->NotifyReplicatedChange();
}
UCC_BuffComponent::UCC_BuffComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    ReplicatedBuffs.Owner = this;
}
void UCC_BuffComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Character = Cast<ACC_BaseCharacter>(GetOwner()))
    {
        Character->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::InitializeASC);
        InitializeASC(Character->GetAbilitySystemComponent(), Character->GetAttributeSet());
    }
}
bool UCC_BuffComponent::CanApply() const
{
    const auto* Character = Cast<ACC_BaseCharacter>(GetOwner());
    return !bEnded && !bClearing && Character && Character->HasAuthority() && Character->IsAlive()
        && !Character->IsActorBeingDestroyed() && BoundASC.IsValid() && BoundASC->GetAvatarActor() == Character;
}
void UCC_BuffComponent::UnbindASC()
{
    if (auto* ASC = BoundASC.Get())
    {
        ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
        ASC->OnGameplayEffectAppliedDelegateToSelf.RemoveAll(this);
        ASC->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
        ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute()).RemoveAll(this);
        for (const auto& Pair : Instances)
            if (auto* Events = ASC->GetActiveEffectEventSet(Pair.Key))
            {
                Events->OnStackChanged.RemoveAll(this);
                Events->OnTimeChanged.RemoveAll(this);
                Events->OnInhibitionChanged.RemoveAll(this);
            }
    }
}
void UCC_BuffComponent::InitializeASC(UAbilitySystemComponent* ASC, UAttributeSet* Attributes)
{
    if (bEnded || BoundASC.Get() == ASC) return;
    ClearBuffs(ECC_BuffEndReason::ASCReplaced);
    UnbindASC();
    BoundASC = ASC;
    if (!ASC) return;
    if (BaseWalkSpeed <= 0.f)
        if (auto* Character = Cast<ACC_BaseCharacter>(GetOwner())) BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
    ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute()).AddUObject(this, &ThisClass::UpdateMovementSpeed);
    UpdateMovementSpeed(FOnAttributeChangeData());
    if (!GetOwner()->HasAuthority()) return;
    ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
    ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
    ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleEffectRemoved);
    const auto Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
    for (auto Handle : Handles)
        if (const auto* Effect = ASC->GetActiveGameplayEffect(Handle))
        {
            const FGameplayEffectSpec Spec = Effect->Spec;
            HandleEffectAdded(ASC, Spec, Handle);
        }
}
FActiveGameplayEffectHandle UCC_BuffComponent::ApplyBuff(TSubclassOf<UGameplayEffect> EffectClass, UAbilitySystemComponent* SourceASC, float Level)
{
    return ApplyBuffWithParameters(EffectClass, SourceASC, Level, {});
}
FActiveGameplayEffectHandle UCC_BuffComponent::ApplyBuffWithParameters(TSubclassOf<UGameplayEffect> EffectClass,
    UAbilitySystemComponent* SourceASC, float Level, const TMap<FGameplayTag, float>& SetByCaller)
{
    if (!CanApply() || !EffectClass || EffectClass->HasAnyClassFlags(CLASS_Abstract) || !FMath::IsFinite(Level) || Level <= 0.f) return {};
    const auto* Definition = EffectClass.GetDefaultObject();
    const auto* Rules = Definition->FindComponent<UCC_BuffRulesComponent>();
    if (!Rules && !Definition->FindComponent<UCC_BuffUIData>()) return {};
    if (Rules)
    {
        if (Definition->DurationPolicy == EGameplayEffectDurationType::Instant && !Rules->Behaviors.IsEmpty()) return {};
        for (const UCC_BuffBehavior* Behavior : Rules->Behaviors)
            if (!Behavior || Behavior->GetClass()->HasAnyClassFlags(CLASS_Abstract)) return {};
    }
    for (const auto& Pair : SetByCaller) if (!Pair.Key.IsValid() || !FMath::IsFinite(Pair.Value)) return {};
    if (!SourceASC) SourceASC = BoundASC.Get();
    if (!IsValid(SourceASC) || !SourceASC->IsOwnerActorAuthoritative()) return {};
    auto Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(Definition);
    auto Spec = SourceASC->MakeOutgoingSpec(EffectClass, Level, Context);
    if (!Spec.IsValid()) return {};
    for (const auto& Pair : SetByCaller) Spec.Data->SetSetByCallerMagnitude(Pair.Key, Pair.Value);
    return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, BoundASC.Get());
}
void UCC_BuffComponent::HandleEffectAdded(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
    if (bEnded || !GetOwner()->HasAuthority() || !BoundASC.IsValid() || !Spec.Def
        || !BoundASC->GetActiveGameplayEffect(Handle)) return;
    const auto* Rules = Spec.Def->FindComponent<UCC_BuffRulesComponent>();
    if (!Rules && !Spec.Def->FindComponent<UCC_BuffUIData>()) return;
    if (!Instances.Contains(Handle))
    {
        FCC_BuffInstance Instance;
        Instance.View.Handle = Handle;
        Instance.View.InstanceId = NextInstanceId++;
        Instance.bRemoveOnDeath = !Rules || Rules->bRemoveOnDeath;
        if (Rules) for (UCC_BuffBehavior* Template : Rules->Behaviors)
            if (Template && !Template->GetClass()->HasAnyClassFlags(CLASS_Abstract))
                Instance.Behaviors.Add(DuplicateObject<UCC_BuffBehavior>(Template, this,
                    MakeUniqueObjectName(this, Template->GetClass(), Template->GetFName())));
        Instances.Add(Handle, MoveTemp(Instance));
        if (auto* Events = BoundASC->GetActiveEffectEventSet(Handle))
        {
            Events->OnStackChanged.AddUObject(this, &ThisClass::HandleStackChanged);
            Events->OnTimeChanged.AddUObject(this, &ThisClass::HandleTimeChanged);
            Events->OnInhibitionChanged.AddUObject(this, &ThisClass::HandleInhibitionChanged);
        }
    }
    RefreshFromGAS(Handle);
}
void UCC_BuffComponent::RefreshFromGAS(FActiveGameplayEffectHandle Handle, bool bRefreshed)
{
    auto* Instance = Instances.Find(Handle);
    const auto* Effect = BoundASC.IsValid() ? BoundASC->GetActiveGameplayEffect(Handle) : nullptr;
    if (!Instance || !Effect) return;
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
    Item.EndServerTime = Item.Duration < 0.f ? -1.f : ServerTime() +
        FMath::Max(0.f, Effect->StartWorldTime + Item.Duration - GetWorld()->GetTimeSeconds());
    Instance->View = View;
    const bool bEnable = !Item.bIsInhibited && !Instance->bSuspendedForDeath && !bEnded;
    TArray<TStrongObjectPtr<UCC_BuffBehavior>> Behaviors;
    for (UCC_BuffBehavior* Behavior : Instance->Behaviors) Behaviors.Emplace(Behavior);
    if (!bEnable) SetHooks(Handle, false);
    for (const auto& Pin : Behaviors)
    {
        auto* Behavior = Pin.Get();
        if (!Instances.Contains(Handle)) return;
        if (Behavior->State == ECC_BuffBehaviorState::Uninitialized) Behavior->Initialize(this, View);
        else
        {
            if (!bEnable) Behavior->Disable(ECC_BuffDisableReason::Inhibited);
            if (!Instances.Contains(Handle)) return;
            Behavior->UpdateView(View, bRefreshed);
        }
        if (!Instances.Contains(Handle)) return;
        if (bEnable) Behavior->Enable();
    }
    Instance = Instances.Find(Handle);
    Effect = BoundASC.IsValid() ? BoundASC->GetActiveGameplayEffect(Handle) : nullptr;
    if (!Instance || !Effect) return;
    // A callback may stack, inhibit or remove the effect. Nested refresh owns newer state.
    if (Effect->Spec.GetStackCount() != View.StackCount || Effect->bIsInhibited != Item.bIsInhibited) return;
    SetHooks(Handle, bEnable);
    auto* Existing = ReplicatedBuffs.Items.FindByPredicate([&](const auto& Entry) { return Entry.InstanceId == Item.InstanceId; });
    if (!Existing) Existing = &ReplicatedBuffs.Items.AddDefaulted_GetRef();
    // Preserve FastArray replication identity while replacing display fields.
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
void UCC_BuffComponent::SetHooks(FActiveGameplayEffectHandle Handle, bool bEnabled)
{
    for (auto& Pair : HookIndex) Pair.Value.RemoveAll([Handle](const auto& Entry) { return Entry.Handle == Handle; });
    const auto* Instance = Instances.Find(Handle);
    if (!Instance || !bEnabled) return;
    for (int32 Index = 0; Index < Instance->Behaviors.Num(); ++Index)
    {
        UCC_BuffBehavior* Behavior = Instance->Behaviors[Index];
        if (!Behavior->IsEnabled()) continue;
        for (auto Tag : Behavior->HandledHooks)
        {
            auto& Entries = HookIndex.FindOrAdd(Tag);
            Entries.Add({Behavior, Handle, Behavior->HookPriority, Instance->View.InstanceId, Index});
            Entries.Sort([](const auto& A, const auto& B)
            {
                if (A.Priority != B.Priority) return A.Priority < B.Priority;
                if (A.InstanceId != B.InstanceId) return A.InstanceId < B.InstanceId;
                return A.BehaviorIndex < B.BehaviorIndex;
            });
        }
    }
}
void UCC_BuffComponent::RunHook(FGameplayTag Hook, FInstancedStruct& Context)
{
    if (!GetOwner()->HasAuthority() || bEnded || bClearing || !Context.IsValid() || HookDepth >= 8) return;
    const auto* Entries = HookIndex.Find(Hook);
    if (!Entries) return;
    const auto Snapshot = *Entries; // New entries join on the NEXT invocation.
    const auto* ExpectedType = Context.GetScriptStruct();
    TGuardValue<int32> DepthGuard(HookDepth, HookDepth + 1);
    for (const auto& Entry : Snapshot)
    {
        TStrongObjectPtr<UCC_BuffBehavior> Behavior(Entry.Behavior.Get());
        if (!Behavior.IsValid() || !Behavior->IsEnabled() || !Instances.Contains(Entry.Handle)) continue;
        const auto Before = Context;
        Behavior->ModifyHook(Hook, Context);
        if (Context.GetScriptStruct() != ExpectedType)
        {
            Context = Before; // A generic hook may modify fields, never replace the context type.
            ensureMsgf(false, TEXT("Buff hook replaced its context type."));
        }
    }
}
bool UCC_BuffComponent::RemoveBuff(int32 InstanceId)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid()) return false;
    for (const auto& Pair : Instances) if (Pair.Value.View.InstanceId == InstanceId)
    {
        const auto Handle = Pair.Key;
        return BoundASC->RemoveActiveGameplayEffect(Handle, -1);
    }
    return false;
}
bool UCC_BuffComponent::ConsumeStacks(FActiveGameplayEffectHandle Handle, int32 Count)
{
    return GetOwner()->HasAuthority() && BoundASC.IsValid() && Count > 0 && Instances.Contains(Handle)
        && BoundASC->RemoveActiveGameplayEffect(Handle, Count);
}
int32 UCC_BuffComponent::RemoveBuffsByQuery(const FGameplayTagQuery& Query, int32 MaxCount)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid() || MaxCount == 0 || MaxCount < -1) return 0;
    TArray<int32> Ids;
    for (const auto& Item : ReplicatedBuffs.Items) if (Query.IsEmpty() || Query.Matches(Item.BuffTags)) Ids.Add(Item.InstanceId);
    Ids.Sort(); // Deterministic oldest-first removal.
    int32 Removed = 0;
    for (int32 Id : Ids) if ((MaxCount < 0 || Removed < MaxCount) && RemoveBuff(Id)) ++Removed;
    return Removed;
}
int32 UCC_BuffComponent::GetStackCountByTag(FGameplayTag Tag, bool bIncludeInhibited) const
{
    int32 Count = 0;
    for (const auto& Item : ReplicatedBuffs.Items)
        if (Item.BuffTags.HasTag(Tag) && (bIncludeInhibited || !Item.bIsInhibited)) Count += Item.StackCount;
    return Count;
}
void UCC_BuffComponent::HandleEffectRemoved(const FActiveGameplayEffect& Effect)
{
    const auto* Reason = RemovalReasons.Find(Effect.Handle);
    const auto EndReason = Reason ? *Reason : ECC_BuffEndReason::EffectRemoved;
    RemovalReasons.Remove(Effect.Handle);
    FinishInstance(Effect.Handle, EndReason);
}
void UCC_BuffComponent::FinishInstance(FActiveGameplayEffectHandle Handle, ECC_BuffEndReason Reason)
{
    FCC_BuffInstance Instance;
    if (!Instances.RemoveAndCopyValue(Handle, Instance)) return;
    SetHooks(Handle, false);
    if (BoundASC.IsValid()) if (auto* Events = BoundASC->GetActiveEffectEventSet(Handle))
    {
        Events->OnStackChanged.RemoveAll(this);
        Events->OnTimeChanged.RemoveAll(this);
        Events->OnInhibitionChanged.RemoveAll(this);
    }
    ReplicatedBuffs.Items.RemoveAll([&](const auto& Item) { return Item.InstanceId == Instance.View.InstanceId; });
    ReplicatedBuffs.MarkArrayDirty();
    TArray<TStrongObjectPtr<UCC_BuffBehavior>> Behaviors;
    for (UCC_BuffBehavior* Behavior : Instance.Behaviors) Behaviors.Emplace(Behavior);
    for (const auto& Pin : Behaviors)
    {
        Pin->Shutdown(Reason);
    }
    NotifyChanged();
}
void UCC_BuffComponent::ClearBuffs(ECC_BuffEndReason Reason)
{
    if (!GetOwner()->HasAuthority() || bClearing) return;
    TGuardValue<bool> Guard(bClearing, true);
    TArray<FActiveGameplayEffectHandle> Handles;
    Instances.GetKeys(Handles);
    for (auto Handle : Handles)
    {
        auto* Instance = Instances.Find(Handle);
        if (!Instance) continue;
        if (Reason == ECC_BuffEndReason::OwnerDeath && !Instance->bRemoveOnDeath)
        {
            Instance->bSuspendedForDeath = true;
            const auto Behaviors = Instance->Behaviors;
            SetHooks(Handle, false);
            for (UCC_BuffBehavior* Behavior : Behaviors)
            {
                TStrongObjectPtr<UCC_BuffBehavior> Pin(Behavior);
                Behavior->Disable(ECC_BuffDisableReason::OwnerDeath);
            }
            continue;
        }
        RemovalReasons.Add(Handle, Reason);
        if (BoundASC.IsValid()) BoundASC->RemoveActiveGameplayEffect(Handle, -1);
        RemovalReasons.Remove(Handle);
        FinishInstance(Handle, Reason); // Also handles expired/detached ASC.
    }
}
void UCC_BuffComponent::HandleStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount) { RefreshFromGAS(Handle); }
void UCC_BuffComponent::HandleTimeChanged(FActiveGameplayEffectHandle Handle, float Start, float Duration) { RefreshFromGAS(Handle, true); }
void UCC_BuffComponent::HandleInhibitionChanged(FActiveGameplayEffectHandle Handle, bool bInhibited) { RefreshFromGAS(Handle); }
float UCC_BuffComponent::ServerTime() const
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}
void UCC_BuffComponent::UpdateMovementSpeed(const FOnAttributeChangeData& Data)
{
    if (auto* Character = Cast<ACC_BaseCharacter>(GetOwner()))
        if (BoundASC.IsValid()) Character->GetCharacterMovement()->MaxWalkSpeed =
            BaseWalkSpeed * BoundASC->GetNumericAttribute(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute());
}
float UCC_BuffComponent::GetRemainingTime(int32 InstanceId) const
{
    const auto* Item = ReplicatedBuffs.Items.FindByPredicate([InstanceId](const auto& Entry) { return Entry.InstanceId == InstanceId; });
    return !Item ? 0.f : (Item->EndServerTime < 0.f ? -1.f : FMath::Max(0.f, Item->EndServerTime - ServerTime()));
}
void UCC_BuffComponent::NotifyChanged() { if (!bEnded) { GetOwner()->ForceNetUpdate(); OnBuffsChanged.Broadcast(); } }
void UCC_BuffComponent::NotifyReplicatedChange() { OnBuffsChanged.Broadcast(); }
void UCC_BuffComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, ReplicatedBuffs);
}
void UCC_BuffComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnded = true;
    ClearBuffs(ECC_BuffEndReason::EndPlay);
    UnbindASC();
    BoundASC.Reset();
    if (auto* Character = Cast<ACC_BaseCharacter>(GetOwner())) Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::InitializeASC);
    Super::EndPlay(Reason);
}
