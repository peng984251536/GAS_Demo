#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/Buff/CC_BuffTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "CC_BuffComponent.generated.h"

class UAttributeSet;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_BuffsChanged);

/** Server instances and hook dispatch; GAS owns modifiers, stacking, duration and tags. */
UCLASS(ClassGroup=(GAS), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_BuffComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCC_BuffComponent();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff")
    FActiveGameplayEffectHandle ApplyBuff(TSubclassOf<UGameplayEffect> EffectClass,
        UAbilitySystemComponent* SourceASC = nullptr, float Level = 1.f);
    FActiveGameplayEffectHandle ApplyBuffWithParameters(TSubclassOf<UGameplayEffect> EffectClass,
        UAbilitySystemComponent* SourceASC, float Level, const TMap<FGameplayTag, float>& SetByCaller);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff") bool RemoveBuff(int32 InstanceId);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff")
    int32 RemoveBuffsByQuery(const FGameplayTagQuery& Query, int32 MaxCount = -1);
    UFUNCTION(BlueprintPure, Category="Buff") int32 GetStackCountByTag(FGameplayTag Tag, bool bIncludeInhibited = false) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Buff")
    void ClearBuffs(ECC_BuffEndReason Reason = ECC_BuffEndReason::Reset);
    UFUNCTION(BlueprintPure, Category="Buff") TArray<FCC_ReplicatedBuffItem> GetActiveBuffs() const { return ReplicatedBuffs.Items; }
    UFUNCTION(BlueprintPure, Category="Buff") float GetRemainingTime(int32 InstanceId) const;
    UPROPERTY(BlueprintAssignable, Category="Buff") FCC_BuffsChanged OnBuffsChanged;
    UFUNCTION() void InitializeASC(UAbilitySystemComponent* ASC, UAttributeSet* Attributes);
    void RunHook(FGameplayTag Hook, FInstancedStruct& Context);
    template<typename T> void RunHook(FGameplayTag Hook, T& Context)
    {
        FInstancedStruct Value = FInstancedStruct::Make<T>(Context);
        RunHook(Hook, Value);
        if (const T* Result = Value.GetPtr<T>()) Context = *Result;
    }
    bool ConsumeStacks(FActiveGameplayEffectHandle Handle, int32 Count);
    void NotifyReplicatedChange();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FHookEntry
    {
        TWeakObjectPtr<UCC_BuffBehavior> Behavior;
        FActiveGameplayEffectHandle Handle;
        int32 Priority = 0;
        int32 InstanceId = 0;
        int32 BehaviorIndex = 0;
    };
    UPROPERTY(Replicated) FCC_ReplicatedBuffArray ReplicatedBuffs;
    UPROPERTY(Transient) TMap<FActiveGameplayEffectHandle, FCC_BuffInstance> Instances;
    TMap<FGameplayTag, TArray<FHookEntry>> HookIndex;
    TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
    TMap<FActiveGameplayEffectHandle, ECC_BuffEndReason> RemovalReasons;
    int32 NextInstanceId = 1;
    int32 HookDepth = 0;
    bool bClearing = false;
    bool bEnded = false;
    float BaseWalkSpeed = 0.f;
    void UpdateMovementSpeed(const FOnAttributeChangeData& Data);
    void UnbindASC();
    void HandleEffectAdded(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
    void HandleEffectRemoved(const FActiveGameplayEffect& Effect);
    void HandleStackChanged(FActiveGameplayEffectHandle Handle, int32 NewCount, int32 OldCount);
    void HandleTimeChanged(FActiveGameplayEffectHandle Handle, float Start, float Duration);
    void HandleInhibitionChanged(FActiveGameplayEffectHandle Handle, bool bInhibited);
    void RefreshFromGAS(FActiveGameplayEffectHandle Handle, bool bRefreshed = false);
    void FinishInstance(FActiveGameplayEffectHandle Handle, ECC_BuffEndReason Reason);
    void SetHooks(FActiveGameplayEffectHandle Handle, bool bEnabled);
    bool CanApply() const;
    void NotifyChanged();
    float ServerTime() const;
};
