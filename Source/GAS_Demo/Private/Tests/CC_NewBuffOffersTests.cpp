#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CC_AttributeSet.h"
#include "AbilitySystem/Effects/Buff/CC_GE_NewOffers.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Actors/Character/CC_PlayerCharacter.h"
#include "Actors/Projectiles/CC_ProjectileFireContext.h"
#include "Components/CC_BuffComponent.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "Data/Buff/CC_BuffOfferPool.h"
#include "Data/Buff/CC_BuffTags.h"
#include "Damage/CC_DamageStatics.h"
#include "GameplayTags/CC_Tags.h"
#include "Utils/Team/CC_TeamStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCNewBuffOffersTest, "GAS_Demo.Buff.NewOffers.ConfigAndEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCNewBuffOffersTest::RunTest(const FString& Parameters)
{
    auto* Pool = LoadObject<UCC_BuffOfferPool>(nullptr,
        TEXT("/Game/GAS_Demo/GameplayEffects/NewBuff/DA_BuffOfferPool_New.DA_BuffOfferPool_New"));
    if (!TestNotNull(TEXT("Requested pool"), Pool)) return false;
    if (!TestEqual(TEXT("Eight options"), Pool->Entries.Num(), 8)) return false;
    for (const auto& Entry : Pool->Entries) TestTrue(TEXT("Usable option"), UCC_BuffOfferPool::IsOptionUsable(Entry));
    FRandomStream Random(721);
    TestEqual(TEXT("Three cards"), Pool->RollOptions(Random).Num(), 3);
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto MakeCharacter = [World](FVector Location)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* C = World->SpawnActor<ACC_EnemyCharacter>(Location, FRotator::ZeroRotator, Params);
        auto* ASC = C->GetAbilitySystemComponent();
        ASC->AddAttributeSetSubobject(C->GetAttributeSet());
        ASC->InitAbilityActorInfo(C, C);
        C->GetRuntimeData()->SetAlive(true);
        C->GetBuffComponent()->InitializeASC(ASC, C->GetAttributeSet());
        return C;
    };
    auto* Shooter = MakeCharacter(FVector::ZeroVector);
    auto* Target = MakeCharacter(FVector(1000, 0, 0));
    auto* Player = World->SpawnActor<ACC_PlayerCharacter>();
    Shooter->SetTeamOverride(UCC_TeamStatics::GetBaseTeam(Player));
    auto* ASC = Shooter->GetAbilitySystemComponent();
    auto* Buffs = Shooter->GetBuffComponent();
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 100.f);
    auto Attack = Buffs->ApplyBuff(Pool->Entries[0].Effect);
    TestEqual(TEXT("Attack +20"), Shooter->GetAttributeSet()->GetAttackPower(), 120.f);
    Buffs->ApplyBuff(Pool->Entries[0].Effect);
    TestEqual(TEXT("Repeat adds another 20"), Shooter->GetAttributeSet()->GetAttackPower(), 140.f);
    ASC->RemoveActiveGameplayEffect(Attack);
    TestEqual(TEXT("Attack removed symmetrically"), Shooter->GetAttributeSet()->GetAttackPower(), 100.f);
    auto Armor = Buffs->ApplyBuff(Pool->Entries[2].Effect);
    TestTrue(TEXT("Armor state"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));
    Armor = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Armor), true, false);
    TestFalse(TEXT("Armor inhibited"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));
    Armor = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Armor), false, false);
    ASC->RemoveActiveGameplayEffect(Armor);
    TestFalse(TEXT("Armor removed"), ASC->HasMatchingGameplayTag(CCTags::Status::SuperArmor));
    auto Split = Buffs->ApplyBuff(Pool->Entries[5].Effect);
    auto Double = Buffs->ApplyBuff(Pool->Entries[6].Effect);
    auto Fire = [&]() { FCC_ProjectileFireContext F; Buffs->RunHook(CCBuffTags::ProjectileFire, F); return F; };
    TestEqual(TEXT("Split count"), Fire().ProjectileCount, 3);
    TestEqual(TEXT("Two sequential volleys"), Fire().VolleyCount, 2);
    TestTrue(TEXT("Delay between volleys"), FMath::IsNearlyEqual(Fire().VolleyInterval, 0.18f));
    Double = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Double), true, false);
    TestEqual(TEXT("Repeat inhibited"), Fire().VolleyCount, 1);
    Double = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Double), false, false);
    TestEqual(TEXT("Repeat restored"), Fire().VolleyCount, 2);
    ASC->RemoveActiveGameplayEffect(Double);
    ASC->RemoveActiveGameplayEffect(Split);
    TestEqual(TEXT("Split removed"), Fire().ProjectileCount, 1);
    TestEqual(TEXT("Repeat removed"), Fire().VolleyCount, 1);
    auto Charm = Buffs->ApplyBuff(Pool->Entries[3].Effect);
    auto* TargetASC = Target->GetAbilitySystemComponent();
    FCC_DamageRequest Damage;
    Damage.FlatDamage = 1.f;
    Damage.SkillScale = 0.f;
    Damage.CriticalPolicy = ECC_CriticalPolicy::Disabled;
    Damage.SourceTags.AddTag(CCBuffTags::ProjectileDamage);
    TestTrue(TEXT("Initially hostile"), UCC_TeamStatics::IsHostile(Shooter, Target));
    Damage.SourceTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Damage.Source.DoT")));
    UCC_DamageStatics::ApplyDamage(ASC, TargetASC, Damage);
    TestTrue(TEXT("DoT does not charm"), UCC_TeamStatics::IsHostile(Shooter, Target));
    Damage.SourceTags.RemoveTag(FGameplayTag::RequestGameplayTag(TEXT("Damage.Source.DoT")));
    UCC_DamageStatics::ApplyDamage(ASC, TargetASC, Damage);
    TestFalse(TEXT("Damage charms enemy"), UCC_TeamStatics::IsHostile(Shooter, Target));
    TestTrue(TEXT("Charmed tag"), TargetASC->HasMatchingGameplayTag(CCTags::Status::Charmed));
    const auto Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CCTags::Status::Charmed));
    TargetASC->RemoveActiveEffects(Query);
    Charm = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Charm), true, false);
    UCC_DamageStatics::ApplyDamage(ASC, TargetASC, Damage);
    TestTrue(TEXT("Inhibited reaction unsubscribed"), UCC_TeamStatics::IsHostile(Shooter, Target));
    Charm = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Charm), false, false);
    UCC_DamageStatics::ApplyDamage(ASC, TargetASC, Damage);
    TestFalse(TEXT("Reaction restored"), UCC_TeamStatics::IsHostile(Shooter, Target));
    ASC->RemoveActiveGameplayEffect(Charm);
    TestTrue(TEXT("Child charm outlives attack buff"), TargetASC->HasMatchingGameplayTag(CCTags::Status::Charmed));
    TargetASC->RemoveActiveEffects(Query);
    UCC_DamageStatics::ApplyDamage(ASC, TargetASC, Damage);
    TestTrue(TEXT("Removed reaction unsubscribed"), UCC_TeamStatics::IsHostile(Shooter, Target));
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 1000.f);
    ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 500.f);
    auto Regen = Buffs->ApplyBuff(Pool->Entries[1].Effect);
    TestEqual(TEXT("No immediate heal"), Shooter->GetAttributeSet()->GetHealth(), 500.f);
    // Tick the real world timer manager without starting AI or game mode.
    ++GFrameCounter;
    World->Tick(LEVELTICK_All, 0.f);
    for (int32 I = 0; I < 52; ++I) { ++GFrameCounter; World->Tick(LEVELTICK_All, 0.1f); }
    TestEqual(TEXT("Five one-percent heals"), Shooter->GetAttributeSet()->GetHealth(), 550.f);
    TestNull(TEXT("Regen expires"), ASC->GetActiveGameplayEffect(Regen));
    return true;
}
#endif
