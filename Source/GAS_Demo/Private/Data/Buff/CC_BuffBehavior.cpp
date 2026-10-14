#include "Data/Buff/CC_BuffBehavior.h"
#include "Components/CC_BuffComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

UWorld* UCC_BuffBehavior::GetWorld() const
{
    // DuplicateObject may retain archetype flags. The runtime component binding is the authority.
    return !HasAnyFlags(RF_ClassDefaultObject) && Component.IsValid() ? Component->GetWorld() : nullptr;
}
void UCC_BuffBehavior::Initialize(UCC_BuffComponent* InComponent, const FCC_BuffInstanceView& InView)
{
    check(State == ECC_BuffBehaviorState::Uninitialized);
    Component = InComponent;
    View = InView;
    State = ECC_BuffBehaviorState::Disabled;
    OnInitialized();
}
void UCC_BuffBehavior::UpdateView(const FCC_BuffInstanceView& InView, bool bRefreshed)
{
    if (State == ECC_BuffBehaviorState::Finished) return;
    const int32 OldStacks = View.StackCount;
    View = InView;
    if (OldStacks != View.StackCount) OnStackChanged(OldStacks, View.StackCount);
    if (bRefreshed && State != ECC_BuffBehaviorState::Finished) OnRefreshed();
}
void UCC_BuffBehavior::Enable()
{
    if (State != ECC_BuffBehaviorState::Disabled) return;
    State = ECC_BuffBehaviorState::Enabled;
    OnEnabled();
}
void UCC_BuffBehavior::Disable(ECC_BuffDisableReason Reason)
{
    if (!IsEnabled()) return;
    State = ECC_BuffBehaviorState::Disabled;
    OnDisabled(Reason);
    ReleaseResources(false);
}
void UCC_BuffBehavior::Shutdown(ECC_BuffEndReason Reason)
{
    if (State == ECC_BuffBehaviorState::Finished) return;
    const bool bWasEnabled = IsEnabled();
    State = ECC_BuffBehaviorState::Finished; // Teardown can re-enter through child effects.
    if (bWasEnabled) OnDisabled(ECC_BuffDisableReason::Ending);
    OnShutdown(Reason);
    ReleaseResources(true);
    Component.Reset();
}
void UCC_BuffBehavior::ListenGameplayEvent(FGameplayTag Tag)
{
    auto* ASC = View.TargetASC.Get();
    if (!IsEnabled() || !ASC || !Tag.IsValid()) return;
    if (Subscriptions.ContainsByPredicate([Tag](const auto& Entry) { return Entry.Tags.HasTagExact(Tag); })) return;
    FGameplayTagContainer Tags(Tag);
    auto Handle = ASC->AddGameplayEventTagContainerDelegate(Tags,
        FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::ReceiveGameplayEvent));
    Subscriptions.Add({ASC, Tags, Handle});
}
void UCC_BuffBehavior::ReceiveGameplayEvent(FGameplayTag Tag, const FGameplayEventData* Event)
{
    if (IsEnabled() && Event)
    {
        TGuardValue<FGameplayEffectContextHandle> Guard(CurrentEventContext, Event->ContextHandle);
        OnGameplayEvent(Tag, *Event);
    }
}
FTimerHandle UCC_BuffBehavior::StartTimer(float Interval, bool bLoop, const FTimerDelegate& Callback)
{
    FTimerHandle Handle;
    if (!IsEnabled() || !GetWorld() || !FMath::IsFinite(Interval) || Interval <= 0.f) return Handle;
    GetWorld()->GetTimerManager().SetTimer(Handle, Callback, Interval, bLoop);
    Timers.Add(Handle);
    return Handle;
}
AActor* UCC_BuffBehavior::SpawnOwnedActor(TSubclassOf<AActor> Class, const FTransform& Transform, ECC_BuffResourceLifetime Lifetime)
{
    if (!IsEnabled() || !Class || !GetWorld()) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = View.Owner.Get();
    Params.Instigator = Cast<APawn>(View.Owner.Get());
    auto* Actor = GetWorld()->SpawnActor<AActor>(Class, Transform, Params);
    if (Actor && Lifetime != ECC_BuffResourceLifetime::Independent)
    {
        if (IsEnabled()) Actors.Add({Actor, Lifetime});
        else Actor->Destroy(); // BeginPlay may have removed the parent synchronously.
    }
    return Actor;
}
FActiveGameplayEffectHandle UCC_BuffBehavior::ApplyChildEffect(UAbilitySystemComponent* Target, TSubclassOf<UGameplayEffect> Effect,
    float Level, ECC_BuffResourceLifetime Lifetime)
{
    auto* Source = View.SourceASC.Get();
    if (!Source) Source = View.TargetASC.Get();
    if (!IsEnabled() || !IsValid(Source) || !IsValid(Target) || !Effect || !Source->IsOwnerActorAuthoritative()
        || !Target->IsOwnerActorAuthoritative() || !FMath::IsFinite(Level) || Level <= 0.f) return {};
    // A stacked handle can belong to multiple parents, so it cannot be an owned child.
    if (Lifetime != ECC_BuffResourceLifetime::Independent
        && Effect.GetDefaultObject()->StackingType != EGameplayEffectStackingType::None) return {};
    auto Context = CurrentEventContext.IsValid() ? CurrentEventContext.Duplicate() : Source->MakeEffectContext();
    auto Spec = Source->MakeOutgoingSpec(Effect, Level, Context);
    if (!Spec.IsValid()) return {};
    auto Handle = Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
    if (Handle.IsValid() && Lifetime != ECC_BuffResourceLifetime::Independent)
    {
        if (IsEnabled()) Effects.Add({Target, Handle, Lifetime});
        else Target->RemoveActiveGameplayEffect(Handle);
    }
    return Handle;
}
bool UCC_BuffBehavior::ConsumeStacks(int32 Count)
{
    return IsEnabled() && Component.IsValid() && Component->ConsumeStacks(View.Handle, Count);
}
void UCC_BuffBehavior::ReleaseResources(bool bFinal)
{
    auto OldSubscriptions = MoveTemp(Subscriptions);
    auto OldTimers = MoveTemp(Timers);
    for (const auto& Entry : OldSubscriptions)
        if (auto* ASC = Entry.ASC.Get()) ASC->RemoveGameplayEventTagContainerDelegate(Entry.Tags, Entry.Handle);
    if (GetWorld()) for (auto& Handle : OldTimers) GetWorld()->GetTimerManager().ClearTimer(Handle);
    TArray<FOwnedActor> ActorsToRelease;
    TArray<FChildEffect> EffectsToRelease;
    // Extract first, because destruction and GE removal invoke synchronous callbacks.
    for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
        if (bFinal || Actors[Index].Lifetime == ECC_BuffResourceLifetime::WhileEnabled)
        { ActorsToRelease.Add(Actors[Index]); Actors.RemoveAt(Index); }
    for (int32 Index = Effects.Num() - 1; Index >= 0; --Index)
        if (bFinal || Effects[Index].Lifetime == ECC_BuffResourceLifetime::WhileEnabled)
        { EffectsToRelease.Add(Effects[Index]); Effects.RemoveAt(Index); }
    for (const auto& Entry : ActorsToRelease) if (auto* Actor = Entry.Actor.Get()) Actor->Destroy();
    for (const auto& Entry : EffectsToRelease) if (auto* ASC = Entry.ASC.Get()) ASC->RemoveActiveGameplayEffect(Entry.Handle);
}
