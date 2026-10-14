#pragma once
#include "CoreMinimal.h"
#include "Data/Buff/CC_BuffTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "TimerManager.h"
#include "CC_BuffBehavior.generated.h"

/** Templates are read-only. Only BuffComponent drives the lifecycle of server instances. */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class GAS_DEMO_API UCC_BuffBehavior : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Hooks") FGameplayTagContainer HandledHooks;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Hooks") int32 HookPriority = 0;
    virtual UWorld* GetWorld() const override;
    bool IsEnabled() const { return State == ECC_BuffBehaviorState::Enabled; }
    const FCC_BuffInstanceView& GetContext() const { return View; }
protected:
    UFUNCTION(BlueprintNativeEvent, Category="Buff") void OnInitialized();
    virtual void OnInitialized_Implementation() {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff") void OnEnabled();
    virtual void OnEnabled_Implementation() {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff") void OnDisabled(ECC_BuffDisableReason Reason);
    virtual void OnDisabled_Implementation(ECC_BuffDisableReason Reason) {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff") void OnStackChanged(int32 OldCount, int32 NewCount);
    virtual void OnStackChanged_Implementation(int32 OldCount, int32 NewCount) {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff") void OnRefreshed();
    virtual void OnRefreshed_Implementation() {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff") void OnShutdown(ECC_BuffEndReason Reason);
    virtual void OnShutdown_Implementation(ECC_BuffEndReason Reason) {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff|Hooks") void ModifyHook(FGameplayTag Hook, UPARAM(ref) FInstancedStruct& Context);
    virtual void ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context) {}
    UFUNCTION(BlueprintNativeEvent, Category="Buff|Events") void OnGameplayEvent(FGameplayTag Tag, const FGameplayEventData& Event);
    virtual void OnGameplayEvent_Implementation(FGameplayTag Tag, const FGameplayEventData& Event) {}
    UFUNCTION(BlueprintPure, Category="Buff") FCC_BuffInstanceView GetInstanceView() const { return View; }
    UFUNCTION(BlueprintCallable, Category="Buff|Resources") void ListenGameplayEvent(FGameplayTag Tag);
    FTimerHandle StartTimer(float Interval, bool bLoop, const FTimerDelegate& Callback);
    UFUNCTION(BlueprintCallable, Category="Buff|Resources") AActor* SpawnOwnedActor(TSubclassOf<AActor> Class, const FTransform& Transform,
        ECC_BuffResourceLifetime Lifetime = ECC_BuffResourceLifetime::Instance);
    UFUNCTION(BlueprintCallable, Category="Buff|Resources") FActiveGameplayEffectHandle ApplyChildEffect(UAbilitySystemComponent* Target,
        TSubclassOf<UGameplayEffect> Effect, float Level = 1.f,
        ECC_BuffResourceLifetime Lifetime = ECC_BuffResourceLifetime::Independent);
    UFUNCTION(BlueprintCallable, Category="Buff") bool ConsumeStacks(int32 Count = 1);
private:
    friend class UCC_BuffComponent;
    void Initialize(UCC_BuffComponent* Component, const FCC_BuffInstanceView& InView);
    void UpdateView(const FCC_BuffInstanceView& InView, bool bRefreshed);
    void Enable();
    void Disable(ECC_BuffDisableReason Reason);
    void Shutdown(ECC_BuffEndReason Reason);
    void ReceiveGameplayEvent(FGameplayTag Tag, const FGameplayEventData* Event);
    void ReleaseResources(bool bFinal);
    UPROPERTY(Transient) FCC_BuffInstanceView View;
    UPROPERTY(Transient) TWeakObjectPtr<UCC_BuffComponent> Component;
    UPROPERTY(Transient) ECC_BuffBehaviorState State = ECC_BuffBehaviorState::Uninitialized;
    struct FEventSubscription { TWeakObjectPtr<UAbilitySystemComponent> ASC; FGameplayTagContainer Tags; FDelegateHandle Handle; };
    struct FOwnedActor { TWeakObjectPtr<AActor> Actor; ECC_BuffResourceLifetime Lifetime; };
    struct FChildEffect { TWeakObjectPtr<UAbilitySystemComponent> ASC; FActiveGameplayEffectHandle Handle; ECC_BuffResourceLifetime Lifetime; };
    TArray<FEventSubscription> Subscriptions;
    TArray<FTimerHandle> Timers;
    TArray<FOwnedActor> Actors;
    TArray<FChildEffect> Effects;
    FGameplayEffectContextHandle CurrentEventContext;
};
