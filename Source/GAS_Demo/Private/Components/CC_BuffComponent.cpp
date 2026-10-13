#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffDefinition.h"
#include "Data/Buff/CC_BuffBehavior.h"
#include "Effects/Buff/CC_BuffFactory.h"
#include "Effects/Buff/CC_GE_Buff.h"
#include "Character/CC_BaseCharacter.h"
#include "Attribute/CC_AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UCC_BuffComponent::UCC_BuffComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
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

void UCC_BuffComponent::UnbindASC()
{
    auto* ASC = BoundASC.Get();
    if (!ASC) return;
    ASC->OnActiveGameplayEffectAddedDelegateToSelf.RemoveAll(this);
    ASC->OnGameplayEffectAppliedDelegateToSelf.RemoveAll(this);
    ASC->OnAnyGameplayEffectRemovedDelegate().RemoveAll(this);
    ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute()).RemoveAll(this);
    for (const auto& Entry : Runtime)
        if (auto* Events = ASC->GetActiveEffectEventSet(Entry.Effect))
        {
            Events->OnStackChanged.RemoveAll(this);
            Events->OnTimeChanged.RemoveAll(this);
            Events->OnInhibitionChanged.RemoveAll(this);
        }
}

void UCC_BuffComponent::InitializeASC(UAbilitySystemComponent* ASC, UAttributeSet* Attributes)
{
    if (BoundASC.Get() == ASC) return;
    if (BoundASC.IsValid()) { ClearBuffs(); UnbindASC(); }
    BoundASC = ASC;
    if (!ASC) return;
    if (!bBaseSpeedCaptured)
        if (auto* Character = Cast<ACC_BaseCharacter>(GetOwner()))
        {
            BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
            bBaseSpeedCaptured = true;
        }
    ASC->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute())
        .AddUObject(this, &ThisClass::UpdateMovementSpeed);
    UpdateMovementSpeed(FOnAttributeChangeData());
    if (!GetOwner()->HasAuthority()) return;
    ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
    ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &ThisClass::HandleEffectAdded);
    ASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &ThisClass::HandleEffectRemoved);
    for (const auto Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
        if (const auto* Effect = ASC->GetActiveGameplayEffect(Handle)) HandleEffectAdded(ASC, Effect->Spec, Handle);
}

FActiveGameplayEffectHandle UCC_BuffComponent::ApplyBuff(TSubclassOf<UCC_GE_Buff> BuffEffect,
    UAbilitySystemComponent* SourceASC, float Level)
{
    auto* Character = Cast<ACC_BaseCharacter>(GetOwner());
    if (!Character || !Character->HasAuthority() || !Character->IsAlive() || Character->IsActorBeingDestroyed() || bClearing) return {};
    InitializeASC(Character->GetAbilitySystemComponent(), Character->GetAttributeSet());
    auto* TargetASC = BoundASC.Get();
    if (!TargetASC || TargetASC->GetAvatarActor() != Character) return {};
    if (!SourceASC) SourceASC = TargetASC;
    if (!IsValid(SourceASC) || !SourceASC->IsOwnerActorAuthoritative()) return {};
    auto Spec = UCC_BuffFactory::MakeBuffSpec(SourceASC, BuffEffect, Level);
    if (!Spec.IsValid()) return {};
    // Native GAS handles requirements, immunity, stacking, overflow, durations, executions and GE component events.
    return SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
}

void UCC_BuffComponent::HandleEffectAdded(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid()) return;
    const auto* Buff = Cast<UCC_GE_Buff>(Spec.Def);
    const auto* Active = BoundASC->GetActiveGameplayEffect(Handle);
    if (!Buff || !Buff->BuffConfiguration || !Active) return; // Instant GE is executed, not an active buff.
    auto* Entry = Runtime.FindByPredicate([Handle](const auto& R) { return R.Effect == Handle; });
    if (!Entry)
    {
        const int32 Id = NextInstanceId++;
        auto& NewEntry = Runtime.AddDefaulted_GetRef();
        NewEntry.InstanceId = Id;
        NewEntry.Effect = Handle;
        auto& Info = ActiveBuffs.AddDefaulted_GetRef();
        Info.InstanceId = Id;
        Info.Definition = Buff->BuffConfiguration;
        Info.EffectClass = Buff->GetClass();
        if (auto* Events = BoundASC->GetActiveEffectEventSet(Handle))
        {
            Events->OnStackChanged.AddUObject(this, &ThisClass::HandleStackChanged);
            Events->OnTimeChanged.AddUObject(this, &ThisClass::HandleTimeChanged);
            Events->OnInhibitionChanged.AddUObject(this, &ThisClass::HandleInhibitionChanged);
        }
    }
    RefreshFromGAS(Handle);
}

void UCC_BuffComponent::RefreshFromGAS(FActiveGameplayEffectHandle Handle)
{
    const auto* Entry = Runtime.FindByPredicate([Handle](const auto& R) { return R.Effect == Handle; });
    const auto* Effect = BoundASC.IsValid() ? BoundASC->GetActiveGameplayEffect(Handle) : nullptr;
    if (!Entry || !Effect) return;
    const int32 Id = Entry->InstanceId;
    auto* Info = ActiveBuffs.FindByPredicate([Id](const auto& R) { return R.InstanceId == Id; });
    if (!Info) return;
    Info->StackCount = Effect->Spec.GetStackCount();
    Info->Duration = Effect->GetDuration();
    Info->bIsInhibited = Effect->bIsInhibited;
    Info->EndServerTime = Info->Duration < 0.f ? -1.f
        : ServerTime() + FMath::Max(0.f, Effect->StartWorldTime + Info->Duration - GetWorld()->GetTimeSeconds());
    const bool bInhibited = Info->bIsInhibited;
    if (bInhibited) StopSpecials(Id); else StartSpecials(Id);
    NotifyChanged();
}

void UCC_BuffComponent::StartSpecials(int32 InstanceId)
{
    auto* Entry = Runtime.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
    const auto* Info = ActiveBuffs.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
    if (!Entry || Entry->bSpecialsStarted || !Info || !Info->Definition) return;
    auto* Character = Cast<ACC_BaseCharacter>(GetOwner());
    const auto* Definition = Info->Definition.GetDefaultObject();
    if (!Character || !Character->IsAlive() || !Definition) return;
    Entry->bSpecialsStarted = true;
    for (int32 Index = 0; Definition->SummonClass && Index < FMath::Clamp(Definition->SummonCount, 1, 32); ++Index)
    {
        const float Angle = 2.f * PI * Index / FMath::Max(1, Definition->SummonCount);
        const FVector Position = Character->GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Definition->SummonRadius;
        FActorSpawnParameters Parameters;
        Parameters.Owner = Character;
        Parameters.Instigator = Character;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
        if (auto* Pawn = GetWorld()->SpawnActor<APawn>(Definition->SummonClass, Position, Character->GetActorRotation(), Parameters))
        {
            auto* Current = Runtime.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
            if (!Current || !Current->bSpecialsStarted) { Pawn->Destroy(); return; }
            Current->Summons.Add(Pawn);
            Pawn->SpawnDefaultController();
        }
    }
    for (const auto& Template : Definition->Behaviors)
    {
        auto* Current = Runtime.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
        if (!Current || !Current->bSpecialsStarted) return;
        if (!Template) continue;
        auto* Behavior = DuplicateObject<UCC_BuffBehavior>(Template, this);
        Current->Behaviors.Add(Behavior);
        Behavior->OnApplied(Character, Definition, InstanceId);
    }
}

void UCC_BuffComponent::StopSpecials(int32 InstanceId)
{
    auto* Entry = Runtime.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
    const auto* Info = ActiveBuffs.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
    if (!Entry || !Entry->bSpecialsStarted) return;
    const auto* Definition = Info && Info->Definition ? Info->Definition.GetDefaultObject() : nullptr;
    FCC_BuffRuntime Snapshot = *Entry;
    Entry->bSpecialsStarted = false;
    Entry->Behaviors.Reset();
    Entry->Summons.Reset();
    if (!Definition) return;
    for (const auto& Behavior : Snapshot.Behaviors)
        if (IsValid(Behavior)) Behavior->OnRemoved(Cast<ACC_BaseCharacter>(GetOwner()), Definition, InstanceId);
    if (Definition->bDestroySummonsOnRemoval)
        for (const auto& Pawn : Snapshot.Summons) if (IsValid(Pawn)) Pawn->Destroy();
}

bool UCC_BuffComponent::RemoveBuff(int32 InstanceId)
{
    if (!GetOwner()->HasAuthority() || !BoundASC.IsValid()) return false;
    const auto* Entry = Runtime.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
    return Entry && BoundASC->RemoveActiveGameplayEffect(Entry->Effect, -1);
}

void UCC_BuffComponent::HandleEffectRemoved(const FActiveGameplayEffect& Effect)
{
    const auto* Entry = Runtime.FindByPredicate([&](const auto& R) { return R.Effect == Effect.Handle; });
    if (!GetOwner()->HasAuthority() || !Entry) return;
    const int32 Id = Entry->InstanceId;
    StopSpecials(Id);
    Runtime.RemoveAll([Id](const auto& R) { return R.InstanceId == Id; });
    ActiveBuffs.RemoveAll([Id](const auto& R) { return R.InstanceId == Id; });
    NotifyChanged();
}

void UCC_BuffComponent::ClearBuffs(bool bDeathOnly)
{
    if (!GetOwner()->HasAuthority() || bClearing) return;
    TGuardValue<bool> Guard(bClearing, true);
    const auto Snapshot = ActiveBuffs;
    for (const auto& Info : Snapshot)
        if (!bDeathOnly || (Info.Definition && Info.Definition.GetDefaultObject()->bRemoveOnDeath)) RemoveBuff(Info.InstanceId);
}

void UCC_BuffComponent::HandleStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount) { RefreshFromGAS(Handle); }
void UCC_BuffComponent::HandleTimeChanged(FActiveGameplayEffectHandle Handle, float Start, float Duration) { RefreshFromGAS(Handle); }
void UCC_BuffComponent::HandleInhibitionChanged(FActiveGameplayEffectHandle Handle, bool bInhibited) { RefreshFromGAS(Handle); }

void UCC_BuffComponent::UpdateMovementSpeed(const FOnAttributeChangeData& Data)
{
    auto* Character = Cast<ACC_BaseCharacter>(GetOwner());
    if (Character && BoundASC.IsValid())
        Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * BoundASC->GetNumericAttribute(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute());
}

int32 UCC_BuffComponent::GetBuffStackCount(TSubclassOf<UCC_GE_Buff> BuffEffect) const
{
    int32 Count = 0;
    for (const auto& Info : ActiveBuffs) if (Info.EffectClass == BuffEffect) Count += Info.StackCount;
    return Count;
}

float UCC_BuffComponent::ServerTime() const
{
    const auto* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return State ? State->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f);
}

float UCC_BuffComponent::GetRemainingTime(int32 InstanceId) const
{
    const auto* Info = ActiveBuffs.FindByPredicate([InstanceId](const auto& R) { return R.InstanceId == InstanceId; });
    return !Info ? 0.f : (Info->EndServerTime < 0.f ? -1.f : FMath::Max(0.f, Info->EndServerTime - ServerTime()));
}

UCombatActionSet* UCC_BuffComponent::GetAttackActionSetOverride() const
{
    for (int32 Index = ActiveBuffs.Num() - 1; Index >= 0; --Index)
    {
        const auto& Info = ActiveBuffs[Index];
        const auto* Definition = Info.Definition ? Info.Definition.GetDefaultObject() : nullptr;
        if (!Info.bIsInhibited && Definition && Definition->AttackActionSetOverride) return Definition->AttackActionSetOverride;
    }
    return nullptr;
}

void UCC_BuffComponent::NotifyChanged() { GetOwner()->ForceNetUpdate(); OnBuffsChanged.Broadcast(); }
void UCC_BuffComponent::OnRep_ActiveBuffs() { OnBuffsChanged.Broadcast(); }

void UCC_BuffComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCC_BuffComponent, ActiveBuffs);
}

void UCC_BuffComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearBuffs();
    if (auto* Character = Cast<ACC_BaseCharacter>(GetOwner())) Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::InitializeASC);
    UnbindASC();
    BoundASC.Reset();
    Super::EndPlay(Reason);
}
