#pragma once
#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CC_BuffTypes.generated.h"
class UCC_BuffComponent;
class UCC_BuffBehavior;
UENUM(BlueprintType)
enum class ECC_BuffBehaviorState : uint8 { Uninitialized, Disabled, Enabled, Finished };
UENUM(BlueprintType)
enum class ECC_BuffDisableReason : uint8 { Inhibited, OwnerDeath, Ending };
UENUM(BlueprintType)
enum class ECC_BuffEndReason : uint8 { EffectRemoved, OwnerDeath, Reset, ASCReplaced, EndPlay };
UENUM(BlueprintType)
enum class ECC_BuffResourceLifetime : uint8 { WhileEnabled, Instance, Independent };

/** Value snapshot; never retains a pointer into mutable GAS storage. */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_BuffInstanceView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Owner;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<AActor> Instigator;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<UAbilitySystemComponent> SourceASC;
    UPROPERTY(BlueprintReadOnly) FActiveGameplayEffectHandle Handle;
    UPROPERTY(BlueprintReadOnly) int32 InstanceId = 0;
    UPROPERTY(BlueprintReadOnly) int32 StackCount = 0;
    UPROPERTY(BlueprintReadOnly) float Level = 1.f;
    UPROPERTY(BlueprintReadOnly) TMap<FGameplayTag, float> SetByCaller;
    float GetSetByCaller(FGameplayTag Tag, float Default = 0.f) const
    { const auto* Value = SetByCaller.Find(Tag); return Value ? *Value : Default; }
};

USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_ReplicatedBuffItem : public FFastArraySerializerItem
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 InstanceId = 0;
    UPROPERTY(BlueprintReadOnly) TSubclassOf<UGameplayEffect> EffectClass;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer BuffTags;
    UPROPERTY(BlueprintReadOnly) int32 StackCount = 1;
    UPROPERTY(BlueprintReadOnly) float Level = 1.f;
    UPROPERTY(BlueprintReadOnly) bool bIsInhibited = false;
    UPROPERTY(BlueprintReadOnly) float EndServerTime = -1.f;
    UPROPERTY(BlueprintReadOnly) float Duration = -1.f;
};
USTRUCT()
struct GAS_DEMO_API FCC_ReplicatedBuffArray : public FFastArraySerializer
{
    GENERATED_BODY()
    UPROPERTY() TArray<FCC_ReplicatedBuffItem> Items;
    UPROPERTY(NotReplicated) TObjectPtr<UCC_BuffComponent> Owner;
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& Params)
    { return FastArrayDeltaSerialize<FCC_ReplicatedBuffItem, FCC_ReplicatedBuffArray>(Items, Params, *this); }
    void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};
template<> struct TStructOpsTypeTraits<FCC_ReplicatedBuffArray> : TStructOpsTypeTraitsBase2<FCC_ReplicatedBuffArray>
{ enum { WithNetDeltaSerializer = true }; };
USTRUCT()
struct FCC_BuffInstance
{
    GENERATED_BODY()
    UPROPERTY() FCC_BuffInstanceView View;
    UPROPERTY() TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors;
    bool bRemoveOnDeath = true;
    bool bSuspendedForDeath = false;
};
