#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Damage/CC_LyraStyleDamage.h"
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
    return true;
}

#endif
