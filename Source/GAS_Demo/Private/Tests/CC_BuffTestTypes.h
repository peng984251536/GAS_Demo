#pragma once
#include "GameplayEffect.h"
#include "Data/Buff/CC_BuffBehavior.h"
#include "CC_BuffTestTypes.generated.h"

UCLASS(NotBlueprintable)
class UCC_BuffTestBehavior : public UCC_BuffBehavior
{
    GENERATED_BODY()
public:
    UCC_BuffTestBehavior();
    int32 Initializes = 0, Enables = 0, Disables = 0, Shutdowns = 0, StackChanges = 0, Events = 0, Ticks = 0;
    bool bConsumeOnHook = false;
    TWeakObjectPtr<AActor> EnabledActor, InstanceActor;
protected:
    virtual void OnInitialized_Implementation() override { ++Initializes; }
    virtual void OnEnabled_Implementation() override;
    virtual void OnDisabled_Implementation(ECC_BuffDisableReason Reason) override { ++Disables; }
    virtual void OnShutdown_Implementation(ECC_BuffEndReason Reason) override { ++Shutdowns; }
    virtual void OnStackChanged_Implementation(int32 OldCount, int32 NewCount) override { ++StackChanges; }
    virtual void OnGameplayEvent_Implementation(FGameplayTag Tag, const FGameplayEventData& Event) override { ++Events; }
    virtual void ModifyHook_Implementation(FGameplayTag Hook, FInstancedStruct& Context) override;
};
UCLASS(NotBlueprintable)
class UCC_BuffTestEffect : public UGameplayEffect
{
    GENERATED_BODY()
public: UCC_BuffTestEffect();
};
UCLASS(NotBlueprintable)
class UCC_BuffTestOtherEffect : public UCC_BuffTestEffect { GENERATED_BODY() };
UCLASS(NotBlueprintable)
class UCC_BuffTestPeriodicEffect : public UGameplayEffect
{
    GENERATED_BODY()
public: UCC_BuffTestPeriodicEffect();
};
UCLASS(NotBlueprintable)
class UCC_BuffTestInstantEffect : public UGameplayEffect
{
    GENERATED_BODY()
public: UCC_BuffTestInstantEffect();
};
UCLASS(NotBlueprintable)
class UCC_BuffTestRetainedEffect : public UCC_BuffTestEffect
{
    GENERATED_BODY()
public: UCC_BuffTestRetainedEffect();
};



USTRUCT()
struct FCC_BuffTestHookContext
{
    GENERATED_BODY()
    // 合并：相加；用于验证通用钩子，不依赖战斗机制。
    UPROPERTY() int32 Value = 0;
};
