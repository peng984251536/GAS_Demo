#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Effects/Damage/CC_GE_LyraStyleDamage.h"
#include "Damage/CC_DamageTypes.h"
#include "Damage/CC_DamageStatics.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCLyraStyleDamageIntegrationTest,
    "GAS_Demo.Damage.LyraStyle.BuffsSnapshotAndHealth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCLyraStyleDamageIntegrationTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
        true, ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("Test world"), World))
    {
        return false;
    }
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };

    auto CreateASC = [&]()
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Actor);
        Actor->AddInstanceComponent(ASC);
        ASC->RegisterComponent();
        ASC->InitAbilityActorInfo(Actor, Actor);
        ASC->AddAttributeSetSubobject(NewObject<UCC_AttributeSet>(Actor));
        return ASC;
    };
    UAbilitySystemComponent* Source = CreateASC();
    UAbilitySystemComponent* Target = CreateASC();
    Source->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 100.0f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 200.0f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 200.0f);

    // 用真实 GE 聚合攻击力，确保伤害代码不会把 Buff 再乘一次。
    UGameplayEffect* Buff = NewObject<UGameplayEffect>(GetTransientPackage());
    Buff->DurationPolicy = EGameplayEffectDurationType::Infinite;
    FGameplayModifierInfo& Modifier = Buff->Modifiers.AddDefaulted_GetRef();
    Modifier.Attribute = UCC_AttributeSet::GetAttackPowerAttribute();
    Modifier.ModifierOp = EGameplayModOp::MultiplyAdditive;
    Modifier.ModifierMagnitude = FScalableFloat(1.5f);
    const FActiveGameplayEffectHandle BuffHandle = Source->ApplyGameplayEffectToSelf(Buff, 1.0f, Source->MakeEffectContext());
    TestEqual(TEXT("Buff raises attack to 150"), Source->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 150.0f);

    FGameplayEffectSpecHandle DamageSpec = Source->MakeOutgoingSpec(
        UCC_GE_LyraStyleDamage::StaticClass(), 1.0f, Source->MakeEffectContext());
    if (!TestTrue(TEXT("Damage spec created"), DamageSpec.IsValid()))
    {
        return false;
    }
    DamageSpec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, 0.5f);
    DamageSpec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, 10.0f);
    // 攻击快照应保留 150，而不是下面的新属性值 300。
    Source->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 200.0f);
    Source->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), Target);
    TestEqual(TEXT("Snapshot damage is 85, buff counted once"), Target->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 115.0f);

    Source->RemoveActiveGameplayEffect(BuffHandle);
    Source->ApplyGameplayEffectSpecToTarget(*DamageSpec.Data.Get(), Target);
    TestEqual(TEXT("Existing spec keeps snapshot after buff removal"), Target->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 30.0f);

    FGameplayEffectSpecHandle NewSpec = Source->MakeOutgoingSpec(
        UCC_GE_LyraStyleDamage::StaticClass(), 1.0f, Source->MakeEffectContext());
    NewSpec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, 0.5f);
    NewSpec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, 10.0f);
    Source->ApplyGameplayEffectSpecToTarget(*NewSpec.Data.Get(), Target);
    TestEqual(TEXT("Fresh attack snapshot produces lethal damage without negative health"), Target->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 0.0f);

    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 200.0f);
    NewSpec.Data->SetStackCount(3);
    Source->ApplyGameplayEffectSpecToTarget(*NewSpec.Data.Get(), Target);
    TestEqual(TEXT("Execution stack count does not multiply one hit"), Target->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 90.0f);

    NewSpec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, -1.0f);
    Source->ApplyGameplayEffectSpecToTarget(*NewSpec.Data.Get(), Target);
    TestEqual(TEXT("Invalid negative skill input does not change health"), Target->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 90.0f);

    Source->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 100.f);
    Source->SetNumericAttributeBase(UCC_AttributeSet::GetOutgoingDamageBonusAttribute(), 0.5f);
    Source->SetNumericAttributeBase(UCC_AttributeSet::GetCriticalMultiplierAttribute(), 2.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 1000.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 1000.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetDefenseAttribute(), 30.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(), 0.8f);
    auto* Set = const_cast<UCC_AttributeSet*>(Target->GetSet<UCC_AttributeSet>());
    FCC_DamageResolved Last;
    int32 Events = 0;
    const FDelegateHandle ResolvedHandle = Set->OnDamageResolved.AddLambda([&](const FCC_DamageResolved& Value) { Last = Value; ++Events; });
    ON_SCOPE_EXIT { Set->OnDamageResolved.Remove(ResolvedHandle); };
    FCC_DamageRequest Request;
    Request.SkillScale = 2.f; Request.FlatDamage = 20.f;
    Request.CriticalPolicy = ECC_CriticalPolicy::Forced;
    TestTrue(TEXT("Unified API accepts damage"), UCC_DamageStatics::ApplyDamage(Source, Target, Request) == ECC_DamageApplyStatus::Applied);
    TestEqual(TEXT("Buffs, defense, critical and reduction produce 504"), Last.ResolvedDamage, 504.f);
    TestEqual(TEXT("504 actual health consumed"), Last.HealthDamage, 504.f);
    TestTrue(TEXT("Explicit critical result"), Last.bCritical);
    TestEqual(TEXT("Damage meta cleared"), Set->GetIncomingDamage(), 0.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 5.f);
    UCC_DamageStatics::ApplyDamage(Source, Target, Request);
    TestEqual(TEXT("Overkill retains full resolved damage"), Last.ResolvedDamage, 504.f);
    TestEqual(TEXT("Overkill consumes only five Health"), Last.HealthDamage, 5.f);
    TestTrue(TEXT("Lethal result"), Last.bKilled);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 100.f);
    Target->AddLooseGameplayTag(StandaloneDamageTags::DamageImmunity);
    const int32 BeforeImmune = Events;
    UCC_DamageStatics::ApplyDamage(Source, Target, Request);
    TestEqual(TEXT("Immunity preserves health"), Set->GetHealth(), 100.f);
    TestEqual(TEXT("Immunity produces no damage feedback"), Events, BeforeImmune);
    Target->RemoveLooseGameplayTag(StandaloneDamageTags::DamageImmunity);
    Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
    Request.WeakPointMultiplier = 2.f;
    UCC_DamageStatics::ApplyDamage(Source, Target, Request);
    TestFalse(TEXT("Weak point is not a critical"), Last.bCritical);
    UCC_DamageStatics::ApplyHealing(Source, Target, 2000.f);
    TestEqual(TEXT("Healing clamps to MaxHealth"), Set->GetHealth(), 1000.f);
    TestTrue(TEXT("Healing result has independent category"), Last.bHealing);
    TestEqual(TEXT("Healing meta cleared"), Set->GetIncomingHealing(), 0.f);
    Request.FlatDamage = -1.f;
    TestTrue(TEXT("Negative request is rejected"), UCC_DamageStatics::ApplyDamage(Source, Target, Request) == ECC_DamageApplyStatus::InvalidInput);

    // Defense and incoming multiplier use current target values; source bonus remains snapshot.
    auto CaptureSpec = Source->MakeOutgoingSpec(UCC_GE_LyraStyleDamage::StaticClass(), 1.f, Source->MakeEffectContext());
    Source->SetNumericAttributeBase(UCC_AttributeSet::GetOutgoingDamageBonusAttribute(), 0.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetDefenseAttribute(), 0.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(), 0.5f);
    Source->ApplyGameplayEffectSpecToTarget(*CaptureSpec.Data.Get(), Target);
    TestEqual(TEXT("Source bonus snapshot and live target reduction"), Last.ResolvedDamage, 75.f);
    return true;
}

#endif
