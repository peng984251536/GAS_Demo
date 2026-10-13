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
    const auto* Configuration = GetDefault<UCC_BuffTestConfiguration>();
    TestEqual(TEXT("Native modifiers copied"), EffectTemplate->Modifiers.Num(), 1);
    TestEqual(TEXT("Native stacking copied"), EffectTemplate->StackLimitCount, 2);
    const auto* EffectTags = EffectTemplate->FindComponent<UTargetTagsGameplayEffectComponent>();
    const auto* ConfigTags = Configuration->FindComponent<UTargetTagsGameplayEffectComponent>();
    if (!TestNotNull(TEXT("GE component initialized"), EffectTags)) return false;
    TestTrue(TEXT("Components have effect outer, not configuration outer"), EffectTags != ConfigTags && EffectTags->GetOwner() == EffectTemplate);
    auto Handle = Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestTrue(TEXT("Apply accepted"), Handle.WasSuccessfullyApplied());
    TestEqual(TEXT("Native modifier applied"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 120.f);
    TestTrue(TEXT("Native tag component grants super armor"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));
    const int32 Id = Component->GetActiveBuffs()[0].InstanceId;
    Advance(0.6f);
    auto SameHandle = Component->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestEqual(TEXT("GAS stacks on same handle"), SameHandle, Handle);
    TestEqual(TEXT("Native stack count mirrored for UI"), Component->GetBuffStackCount(UCC_BuffTestEffect::StaticClass()), 2);
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
    TestEqual(TEXT("Different effect classes sharing config remain distinct"), Component->GetActiveBuffs().Num(), 2);
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
#endif
