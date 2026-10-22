#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Mechanics/CC_SummonAlly.h"
#include "AbilitySystem/Effects/Buff/CC_GE_BuffDash.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "Components/CC_SummonBuffShareComponent.h"
#include "Tests/CC_BuffTestTypes.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "GameplayTags/CC_Tags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSummonBuffTest, "GAS_Demo.Buff.Summon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 召唤 Buff 的授予与收回：施加后授予 UCC_SummonAlly，抑制 / 移除时撤回，恢复后重新授予。
 * 原生 UCC_SummonAlly 没有 SummonClass，自动激活后不召唤、不消耗 Buff，便于检查授予与收回。
 * 实际召唤与阵营在 PIE 中验证（阵营逻辑见 GAS_Demo.Team.InstanceOverride）。
 */
bool FCCSummonBuffTest::RunTest(const FString& Parameters)
{
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };

    auto* Character = World->SpawnActor<ACC_EnemyCharacter>();
    if (!TestNotNull(TEXT("Character"), Character)) return false;
    UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
    ASC->AddAttributeSetSubobject(Character->GetAttributeSet());
    ASC->InitAbilityActorInfo(Character, Character);
    Character->GetRuntimeData()->SetAlive(true);
    Character->GetBuffComponent()->InitializeASC(ASC, Character->GetAttributeSet());

    auto Handle = Character->GetBuffComponent()->ApplyBuff(UCC_GE_BuffSummon::StaticClass());
    TestTrue(TEXT("召唤 Buff 已施加"), Handle.IsValid());
    TestNotNull(TEXT("施加后授予召唤能力"), ASC->FindAbilitySpecFromClass(UCC_SummonAlly::StaticClass()));

    Handle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Handle), true, false);
    TestNull(TEXT("抑制时撤回能力"), ASC->FindAbilitySpecFromClass(UCC_SummonAlly::StaticClass()));
    Handle = ASC->SetActiveGameplayEffectInhibit(MoveTemp(Handle), false, false);
    TestNotNull(TEXT("恢复后重新授予"), ASC->FindAbilitySpecFromClass(UCC_SummonAlly::StaticClass()));

    ASC->RemoveActiveGameplayEffect(Handle);
    TestNull(TEXT("移除后撤回能力"), ASC->FindAbilitySpecFromClass(UCC_SummonAlly::StaticClass()));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSummonBuffShareTest, "GAS_Demo.Buff.SummonShare",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 召唤物 Buff 共享：召唤时复制现有 Buff；之后召唤者每获得一次（新增 / 叠层）召唤物也获得一次；
 * 复制品独立存在，召唤者的 Buff 移除不影响召唤物；带 Buff.NotShared 的召唤 Buff 不同步。
 */
bool FCCSummonBuffShareTest::RunTest(const FString& Parameters)
{
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };

    auto MakeCharacter = [World](const FVector& Location)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Character = World->SpawnActor<ACC_EnemyCharacter>(Location, FRotator::ZeroRotator, Params);
        UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
        ASC->AddAttributeSetSubobject(Character->GetAttributeSet());
        ASC->InitAbilityActorInfo(Character, Character);
        Character->GetRuntimeData()->SetAlive(true);
        Character->GetBuffComponent()->InitializeASC(ASC, Character->GetAttributeSet());
        return Character;
    };
    auto* Owner = MakeCharacter(FVector::ZeroVector);
    auto* Summon = MakeCharacter(FVector(300.f, 0.f, 0.f));
    if (!TestNotNull(TEXT("Owner"), Owner) || !TestNotNull(TEXT("Summon"), Summon)) return false;
    UAbilitySystemComponent* OwnerASC = Owner->GetAbilitySystemComponent();
    UAbilitySystemComponent* SummonASC = Summon->GetAbilitySystemComponent();
    auto Count = [](UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> Class)
    {
        return ASC->GetGameplayEffectCount(Class, nullptr);
    };

    const FActiveGameplayEffectHandle OwnerBuff = Owner->GetBuffComponent()->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    auto* Share = NewObject<UCC_SummonBuffShareComponent>(Summon);
    Share->RegisterComponent();
    Share->StartSharing(Owner);
    TestEqual(TEXT("召唤时复制已有 Buff"), Count(SummonASC, UCC_BuffTestEffect::StaticClass()), 1);

    Owner->GetBuffComponent()->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestEqual(TEXT("召唤者叠层，召唤物也叠一层"), Count(SummonASC, UCC_BuffTestEffect::StaticClass()), 2);

    Owner->GetBuffComponent()->ApplyBuff(UCC_BuffTestOtherEffect::StaticClass());
    TestEqual(TEXT("召唤者获得新 Buff，召唤物也获得"), Count(SummonASC, UCC_BuffTestOtherEffect::StaticClass()), 1);

    OwnerASC->RemoveActiveGameplayEffect(OwnerBuff);
    TestEqual(TEXT("召唤者的 Buff 移除不影响召唤物"), Count(SummonASC, UCC_BuffTestEffect::StaticClass()), 2);

    Owner->GetBuffComponent()->ApplyBuff(UCC_GE_BuffSummon::StaticClass());
    TestEqual(TEXT("召唤 Buff（Buff.NotShared）不同步"), Count(SummonASC, UCC_GE_BuffSummon::StaticClass()), 0);
    TestNull(TEXT("召唤物没有被授予召唤能力"), SummonASC->FindAbilitySpecFromClass(UCC_SummonAlly::StaticClass()));

    Summon->GetRuntimeData()->SetAlive(false);
    Owner->GetBuffComponent()->ApplyBuff(UCC_BuffTestEffect::StaticClass());
    TestEqual(TEXT("召唤物死亡后不再接收"), Count(SummonASC, UCC_BuffTestEffect::StaticClass()), 2);
    return true;
}
#endif
