#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/Mechanics/CC_Dash.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Actors/Projectiles/CC_ArrowProjectile.h"
#include "Actors/Projectiles/CC_ProjectileFireContext.h"
#include "Components/CC_BuffComponent.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Damage/CC_DamageStatics.h"
#include "GameplayEffect.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayTags/CC_Tags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffExamplesTest, "GAS_Demo.Buff.FourExamples",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 用一个集成用例检查四种实际 Buff 配置，不运行整个项目测试集。 */
bool FCCBuffExamplesTest::RunTest(const FString& Parameters)
{
    auto LoadBuff = [this](const TCHAR* Name) -> UClass*
    {
        const FString Path = FCString::Strcmp(Name, TEXT("SplitArrow")) == 0
            ? TEXT("/Script/GAS_Demo.CC_GE_BuffSplitArrow")
            : FString::Printf(TEXT("/Game/GAS_Demo/Effects/Buff/GE_Buff_%s.GE_Buff_%s_C"), Name, Name);
        auto* Class = LoadClass<UGameplayEffect>(nullptr, *Path);
        TestNotNull(*Path, Class);
        return Class;
    };
    UClass* Attack = LoadBuff(TEXT("Attack"));
    UClass* Armor = LoadBuff(TEXT("SuperArmor"));
    UClass* Dash = LoadBuff(TEXT("Dash"));
    UClass* Split = LoadBuff(TEXT("SplitArrow"));
    if (!Attack || !Armor || !Dash || !Split) return false;
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto MakeCharacter = [World]()
    {
        auto* Character = World->SpawnActor<ACC_EnemyCharacter>();
        auto* ASC = Character->GetAbilitySystemComponent();
        ASC->AddAttributeSetSubobject(Character->GetAttributeSet());
        ASC->InitAbilityActorInfo(Character, Character);
        Character->GetRuntimeData()->SetAlive(true);
        Character->GetBuffComponent()->InitializeASC(ASC, Character->GetAttributeSet());
        return Character;
    };
    auto* Character = MakeCharacter();
    auto* Other = MakeCharacter();
    auto* ASC = Character->GetAbilitySystemComponent();
    auto* TargetASC = Other->GetAbilitySystemComponent();
    auto* Buffs = Character->GetBuffComponent();
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 100.f);
    const auto AttackHandle = Buffs->ApplyBuff(Attack);
    TestEqual(TEXT("Attack +20"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 120.f);
    const auto ArmorHandle = Buffs->ApplyBuff(Armor);
    TestTrue(TEXT("Super armor grants state"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));
    ASC->RemoveActiveGameplayEffect(ArmorHandle);
    TestFalse(TEXT("Armor removal restores state"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));

    FCC_DamageRequest Request;
    Request.SkillScale = 0.6f;
    Request.FlatDamage = 6.f;
    Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
    Request.SourceTags.AddTag(CCBuffTags::ProjectileDamage);
    const auto Snapshot = UCC_DamageStatics::PrepareDamage(ASC, Request);
    ASC->RemoveActiveGameplayEffect(AttackHandle);
    TestEqual(TEXT("Attack removal restores attribute"), ASC->GetNumericAttribute(UCC_AttributeSet::GetAttackPowerAttribute()), 100.f);
    TargetASC->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 500.f);
    TargetASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 500.f);
    TargetASC->SetNumericAttributeBase(UCC_AttributeSet::GetDefenseAttribute(), 0.f);
    TargetASC->SetNumericAttributeBase(UCC_AttributeSet::GetIncomingDamageMultiplierAttribute(), 1.f);
    TestTrue(TEXT("Prepared damage applied"), UCC_DamageStatics::ApplyPreparedDamage(TargetASC, Snapshot) == ECC_DamageApplyStatus::Applied);
    TestTrue(TEXT("Damage keeps launch attack snapshot"), FMath::IsNearlyEqual(
        TargetASC->GetNumericAttribute(UCC_AttributeSet::GetHealthAttribute()), 422.f));
    TestTrue(TEXT("Snapshot carries projectile source tag"),
        Snapshot.IsValid() && Snapshot.Data->GetDynamicAssetTags().HasTagExact(CCBuffTags::ProjectileDamage));

    auto DashHandle = Buffs->ApplyBuff(Dash);
    auto* DashSpec = ASC->FindAbilitySpecFromClass(UCC_Dash::StaticClass());
    if (!TestNotNull(TEXT("Dash GE grants actual ability"), DashSpec)) return false;
    const auto AbilityHandle = DashSpec->Handle;
    TestTrue(TEXT("Dash activates via generic GAS API"), ASC->TryActivateAbility(AbilityHandle));
    TestTrue(TEXT("Dash creates root motion"), Character->GetCharacterMovement()->GetRootMotionSource(TEXT("BuffDash")).IsValid());
    DashHandle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(DashHandle), true, false);
    TestNull(TEXT("Inhibition cancels and withdraws dash"), ASC->FindAbilitySpecFromClass(UCC_Dash::StaticClass()));
    const auto Motion = Character->GetCharacterMovement()->GetRootMotionSource(TEXT("BuffDash"));
    TestTrue(TEXT("Cancellation reclaims root motion"), !Motion.IsValid() || Motion->Status.HasFlag(ERootMotionSourceStatusFlags::MarkedForRemoval));
    DashHandle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(DashHandle), false, false);
    TestNotNull(TEXT("Recovery grants dash again"), ASC->FindAbilitySpecFromClass(UCC_Dash::StaticClass()));
    ASC->RemoveActiveGameplayEffect(DashHandle);
    TestNull(TEXT("Removal withdraws dash"), ASC->FindAbilitySpecFromClass(UCC_Dash::StaticClass()));

    auto RunFire = [Buffs]()
    {
        FCC_ProjectileFireContext Fire;
        Fire.ProjectileClass = ACC_ArrowProjectile::StaticClass();
        Buffs->RunHook(CCBuffTags::ProjectileFire, Fire);
        return Fire;
    };
    auto SplitHandle = Buffs->ApplyBuff(Split);
    auto Fire = RunFire();
    TestEqual(TEXT("One stack gives three arrows"), Fire.ProjectileCount, 3);
    TestTrue(TEXT("Per-arrow ratio"), FMath::IsNearlyEqual(Fire.Payload.DamageMultiplier, 0.6f));
    TArray<FVector> Directions;
    Fire.Resolve(Directions);
    TestEqual(TEXT("Three concrete directions"), Directions.Num(), 3);
    if (Directions.Num() == 3)
    {
        TestTrue(TEXT("Center arrow keeps aim"), Directions[1].Equals(FVector::ForwardVector));
        TestTrue(TEXT("Fan is symmetric"), FMath::IsNearlyEqual(Directions[0].Y, -Directions[2].Y));
    }
    Buffs->ApplyBuff(Split);
    TestEqual(TEXT("Two stacks gives five arrows"), RunFire().ProjectileCount, 5);
    const auto OtherSourceHandle = Buffs->ApplyBuff(Split, TargetASC);
    Fire = RunFire();
    TestEqual(TEXT("Different sources add arrow counts"), Fire.ProjectileCount, 7);
    TestTrue(TEXT("Different rules multiply ratios"), FMath::IsNearlyEqual(Fire.Payload.DamageMultiplier, 0.36f));
    ASC->RemoveActiveGameplayEffect(OtherSourceHandle);
    SplitHandle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(SplitHandle), true, false);
    TestEqual(TEXT("Inhibited split gives normal arrow"), RunFire().ProjectileCount, 1);
    SplitHandle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(SplitHandle), false, false);
    TestEqual(TEXT("Recovery preserves split stack count"), RunFire().ProjectileCount, 5);
    ASC->RemoveActiveGameplayEffect(SplitHandle);
    TestEqual(TEXT("Removal restores normal arrow"), RunFire().ProjectileCount, 1);
    TestEqual(TEXT("Removal restores damage ratio"), RunFire().Payload.DamageMultiplier, 1.f);
    TestEqual(TEXT("All example buffs cleaned up"), Buffs->GetActiveBuffs().Num(), 0);
    return true;
}
#endif
