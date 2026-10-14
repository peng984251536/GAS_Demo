#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/CC_BuffTestTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Character/CC_EnemyCharacter.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "GameplayTags/CC_Tags.h"
#include "Damage/CC_DamageTypes.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "UObject/UObjectHash.h"
#include "UObject/StrongObjectPtr.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCNativeBuffIntegrationTest, "GAS_Demo.Buff.NativeConfigurationAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 使用真实角色 ASC 验证配置加载、原生修饰器、堆叠和 GE 组件触发流程。
 * 测试同时检查 UI 快照，确保角色组件反映 GAS 状态而不实现另一套规则。
 */
bool FCCNativeBuffIntegrationTest::RunTest(const FString& Parameters)
{
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto Advance = [World](float Seconds)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 0.f);
        // Small steps avoid the world's per-frame delta clamp hiding duration/period transitions.
        while (Seconds > 0.f)
        {
            const float Step = FMath::Min(Seconds, 0.1f);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, Step);
            Seconds -= Step;
        }
    };
    auto* Character = World->SpawnActor<ACC_EnemyCharacter>();
    auto* ASC = Character->GetAbilitySystemComponent();
    // This test world does not begin gameplay; explicitly register the character's existing attribute set.
    ASC->AddAttributeSetSubobject(Character->GetAttributeSet());
    ASC->InitAbilityActorInfo(Character, Character);
    Character->GetRuntimeData()->SetAlive(true);
    auto* Component = Character->GetBuffComponent();
    Component->InitializeASC(ASC, Character->GetAttributeSet());
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 100.f);
    const auto* EffectTemplate = GetDefault<UCC_BuffTestEffect>();
    TestEqual(TEXT("Native modifiers configured"), EffectTemplate->Modifiers.Num(), 1);
    TestEqual(TEXT("Native stacking configured"), EffectTemplate->StackLimitCount, 2);
    const auto* EffectTags = EffectTemplate->FindComponent<UTargetTagsGameplayEffectComponent>();
    if (!TestNotNull(TEXT("GE component initialized"), EffectTags)) return false;
    auto Handle = Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestTrue(TEXT("Apply accepted"), Handle.WasSuccessfullyApplied());
    TestEqual(TEXT("Native modifier applied"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 120.f);
    TestTrue(TEXT("Native tag component grants super armor"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));
    const int32 Id = Component->GetActiveBuffs()[0].InstanceId;
    Advance(0.6f);
    auto SameHandle = Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestEqual(TEXT("GAS stacks on same handle"), SameHandle, Handle);
    TestEqual(TEXT("Native stack count mirrored for UI"), Component->GetStackCountByTag(CCTags::Status::SuperArmor), 2);
    TestEqual(TEXT("Stacked native additive modifiers"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 140.f);
    TestFalse(TEXT("Native overflow rejects application"), Component->ApplyBuff(UCC_BuffTestEffect::StaticClass()).WasSuccessfullyApplied());
    ASC->AddLooseGameplayTag(CCTags::Status::Invincible);
    TestEqual(TEXT("Native ongoing requirements inhibit modifiers"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 100.f);
    TestTrue(TEXT("UI mirrors inhibition"), Component->GetActiveBuffs()[0].bIsInhibited);
    ASC->RemoveLooseGameplayTag(CCTags::Status::Invincible);
    TestEqual(TEXT("Native requirement removal resumes modifiers"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 140.f);
    Advance(0.6f);
    TestTrue(TEXT("Native stacking refreshes duration past original expiry"), Component->GetRemainingTime(Id) > 0.f);
    Advance(0.5f);
    TestEqual(TEXT("Native expiration restores attributes"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 100.f);
    TestEqual(TEXT("Native expiration removes UI record"), Component->GetActiveBuffs().Num(), 0);

    ASC->AddLooseGameplayTag(StandaloneDamageTags::DamageImmunity);
    TestFalse(TEXT("Native application requirement rejects"), Component->ApplyBuff(UCC_BuffTestEffect::StaticClass()).WasSuccessfullyApplied());
    ASC->RemoveLooseGameplayTag(StandaloneDamageTags::DamageImmunity);
    Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    Component->ApplyBuff(UCC_BuffTestOtherEffect::StaticClass());
    TestEqual(TEXT("Different GE classes remain distinct"), Component->GetActiveBuffs().Num(), 2);
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 50.f);
    Component->RemoveBuff(Component->GetActiveBuffs()[0].InstanceId);
    TestEqual(TEXT("Native AdditionalEffects removal trigger executes healing"), ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 60.f);
    Component->ClearBuffs();
    TestEqual(TEXT("Native triggers are not persistent UI buffs"), Component->GetActiveBuffs().Num(), 0);

    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 100.f);
    auto Periodic = Component->ApplyBuff(UCC_BuffTestPeriodicEffect::StaticClass());
    TestTrue(TEXT("Periodic GE accepted"), Periodic.WasSuccessfullyApplied());
    TestEqual(TEXT("Native periodic flag skips initial tick"), ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 100.f);
    Advance(0.3f);
    TestEqual(TEXT("Native Period executes damage modifier"), ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 95.f);
    Component->ClearBuffs();
    Advance(0.3f);
    TestEqual(TEXT("Removal stops native periodic damage"), ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 95.f);
    auto Instant = Component->ApplyBuff(UCC_BuffTestInstantEffect::StaticClass());
    TestTrue(TEXT("Instant success distinguished from active-handle validity"), Instant.WasSuccessfullyApplied());
    TestEqual(TEXT("Instant healing clamped"), ASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 100.f);
    TestEqual(TEXT("Instant effects have no persistent UI row"), Component->GetActiveBuffs().Num(), 0);
    Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    Character->ACC_BaseCharacter::HandleDeath();
    TestEqual(TEXT("Death cleans native buff handles"), Component->GetActiveBuffs().Num(), 0);
    TestEqual(TEXT("Death restores modified attack"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 100.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffBehaviorLifecycleTest, "GAS_Demo.Buff.BehaviorResourcesAndReentrancy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCCBuffBehaviorLifecycleTest::RunTest(const FString& Parameters)
{
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Character = World->SpawnActor<ACC_EnemyCharacter>();
    auto* ASC = Character->GetAbilitySystemComponent();
    ASC->AddAttributeSetSubobject(Character->GetAttributeSet());
    ASC->InitAbilityActorInfo(Character, Character);
    Character->GetRuntimeData()->SetAlive(true);
    auto* Component = Character->GetBuffComponent();
    Component->InitializeASC(ASC, Character->GetAttributeSet());
    auto FindBehavior = [Component](FActiveGameplayEffectHandle Handle)
    {
        UCC_BuffTestBehavior* Found = nullptr;
        ForEachObjectWithOuter(Component, [&Found, Handle](UObject* Object)
        {
            auto* Behavior = Cast<UCC_BuffTestBehavior>(Object);
            if (Behavior && Behavior->GetContext().Handle == Handle) Found = Behavior;
        }, false);
        return Found;
    };
    auto SendEvent = [ASC]
    {
        FGameplayEventData Event;
        ASC->HandleGameplayEvent(CCTags::CCAbilityTrigger::BowShoot, &Event);
    };
    ASC->AddLooseGameplayTag(CCTags::Status::Invincible);
    auto Handle = Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TStrongObjectPtr<UCC_BuffTestBehavior> Behavior(FindBehavior(Handle));
    if (!TestNotNull(TEXT("Instance"), Behavior.Get())) return false;
    TestEqual(TEXT("Initialize exactly once"), Behavior->Initializes, 1);
    TestEqual(TEXT("Initially inhibited never enabled"), Behavior->Enables, 0);
    SendEvent();
    TestEqual(TEXT("Inhibited instance has no event subscription"), Behavior->Events, 0);
    ASC->RemoveLooseGameplayTag(CCTags::Status::Invincible);
    TestEqual(TEXT("Enabled after inhibition ends"), Behavior->Enables, 1);
    TestTrue(TEXT("Enabled-owned actor exists"), Behavior->EnabledActor.IsValid());
    auto Advance = [World](float Seconds)
    {
        ++GFrameCounter;
        World->Tick(LEVELTICK_All, 0.f); // Activate pending timers before advancing the clock.
        while (Seconds > 0.f)
        {
            const float Step = FMath::Min(Seconds, 0.1f);
            ++GFrameCounter;
            World->Tick(LEVELTICK_All, Step);
            Seconds -= Step;
        }
    };
    Advance(0.2f);
    TestTrue(TEXT("Managed timer runs while enabled"), Behavior->Ticks > 0);
    auto InstanceActor = Behavior->InstanceActor;
    SendEvent();
    TestEqual(TEXT("Event received once"), Behavior->Events, 1);
    Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestEqual(TEXT("Stacking keeps exact object identity"), FindBehavior(Handle), Behavior.Get());
    TestEqual(TEXT("Stack callback received"), Behavior->StackChanges, 1);
    auto OtherHandle = Component->ApplyBuff(UCC_BuffTestOtherEffect::StaticClass());
    TestTrue(TEXT("Two active handles have distinct instances"), FindBehavior(OtherHandle) != Behavior.Get());
    ASC->AddLooseGameplayTag(CCTags::Status::Invincible);
    TestFalse(TEXT("Enabled-owned actor reclaimed on inhibition"), Behavior->EnabledActor.IsValid());
    TestTrue(TEXT("Instance-owned resource survives inhibition"), InstanceActor.IsValid());
    const int32 TicksBefore = Behavior->Ticks;
    Advance(0.2f);
    TestEqual(TEXT("Inhibition cancels managed timer"), Behavior->Ticks, TicksBefore);
    SendEvent();
    TestEqual(TEXT("Disabled subscription removed"), Behavior->Events, 1);
    ASC->RemoveLooseGameplayTag(CCTags::Status::Invincible);
    TestEqual(TEXT("Recovery does not reinitialize"), Behavior->Initializes, 1);
    TestEqual(TEXT("Recovery activates same instance"), Behavior->Enables, 2);
    SendEvent();
    TestEqual(TEXT("Recovery does not duplicate subscriptions"), Behavior->Events, 2);
    Behavior->bConsumeOnHook = true;
    FCC_BuffTestHookContext Fire;
    Component->RunHook(CCTags::CCAbilityTrigger::BowShoot, Fire);
    TestEqual(TEXT("Self-removal inside hook shuts down once"), Behavior->Shutdowns, 1);
    TestFalse(TEXT("Final removal reclaims instance-owned resource"), InstanceActor.IsValid());
    TestFalse(TEXT("Final removal reclaims enabled-owned resource"), Behavior->EnabledActor.IsValid());
    const int32 EventCount = Behavior->Events;
    SendEvent();
    TestEqual(TEXT("Finished behavior receives no events"), Behavior->Events, EventCount);
    Component->ClearBuffs();

    auto RetainedHandle = Component->ApplyBuff(UCC_BuffTestRetainedEffect::StaticClass());
    TStrongObjectPtr<UCC_BuffTestBehavior> Retained(FindBehavior(RetainedHandle));
    if (!TestNotNull(TEXT("Retained instance"), Retained.Get())) return false;
    Character->ACC_BaseCharacter::HandleDeath();
    TestEqual(TEXT("Death retains configured GE"), Component->GetActiveBuffs().Num(), 1);
    TestFalse(TEXT("Retained behavior stops running on corpse"), Retained->IsEnabled());
    Component->ClearBuffs(ECC_BuffEndReason::EndPlay);
    TestEqual(TEXT("EndPlay shuts retained instance down"), Retained->Shutdowns, 1);
    TestEqual(TEXT("EndPlay clears retained GE"), Component->GetActiveBuffs().Num(), 0);
    TestEqual(TEXT("Template remains uninitialized"), GetDefault<UCC_BuffTestBehavior>()->Initializes, 0);
    return true;
}

#endif
