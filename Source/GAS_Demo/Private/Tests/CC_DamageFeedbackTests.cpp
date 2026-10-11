#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Damage/CC_DamageStatics.h"
#include "Damage/CC_CombatFeedbackComponent.h"
#include "Damage/CC_GameplayEffectContext.h"
#include "GameplayCues/CC_GameplayCueManager.h"
#include "GameplayCues/CC_GameplayCue_DamageText.h"
#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/NameAsStringProxyArchive.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCDamageFeedbackTest, "GAS_Demo.Damage.FeedbackAndGrouping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCCDamageFeedbackTest::RunTest(const FString& Parameters)
{
    const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("World"), World)) return false;
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
    auto* Viewer = World->SpawnActor<APlayerController>();
    auto* OtherViewer = World->SpawnActor<APlayerController>();
    // The isolated test world has no GameMode/PostLogin to register controllers.
    World->AddController(Viewer);
    World->AddController(OtherViewer);
    auto CreateASC = [&]()
    {
        auto* Actor = World->SpawnActor<APawn>();
        auto* ASC = NewObject<UCC_AbilitySystemComponent>(Actor);
        Actor->AddInstanceComponent(ASC);
        ASC->RegisterComponent();
        ASC->AddAttributeSetSubobject(NewObject<UCC_AttributeSet>(Actor));
        ASC->InitAbilityActorInfo(Actor, Actor);
        return ASC;
    };
    auto* Source = CreateASC();
    auto* Target = CreateASC();
    Viewer->Possess(Cast<APawn>(Source->GetAvatarActor()));
    Source->SetNumericAttributeBase(UCC_AttributeSet::GetAttackPowerAttribute(), 10.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 1000.f);
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 1000.f);
    auto* Text = World->GetSubsystem<UCC_DamageTextSubsystem>();
    if (!TestNotNull(TEXT("Damage text subsystem"), Text)) return false;
    TestNotNull(TEXT("Automatic feedback component"), Target->CombatFeedback.Get());
    TestNotNull(TEXT("Configured project Context allocation"), FCC_GameplayEffectContext::Get(Source->MakeEffectContext()));
    // Serialize presentation metadata independently of actor relevancy/package maps.
    FCC_GameplayEffectContext OriginalContext;
    OriginalContext.DamageType = CCDamageCueTags::DamageText;
    OriginalContext.bCritical = true;
    OriginalContext.bUseHitLocation = true;
    TArray<uint8> Bytes;
    FMemoryWriter MemoryWriter(Bytes);
    FNameAsStringProxyArchive Writer(MemoryWriter);
    bool bWriteSuccess = false;
    OriginalContext.NetSerialize(Writer, nullptr, bWriteSuccess);
    FMemoryReader MemoryReader(Bytes);
    FNameAsStringProxyArchive Reader(MemoryReader);
    FCC_GameplayEffectContext ReceivedContext;
    bool bReadSuccess = false;
    ReceivedContext.NetSerialize(Reader, nullptr, bReadSuccess);
    TestTrue(TEXT("Context network metadata serializes successfully"), bWriteSuccess && bReadSuccess);
    TestTrue(TEXT("Context retains explicit critical and anchor policy"), ReceivedContext.bCritical && ReceivedContext.bUseHitLocation);
    TestTrue(TEXT("Context retains damage category"), ReceivedContext.DamageType == OriginalContext.DamageType);
    FHitResult OriginalHit;
    OriginalHit.ImpactPoint = FVector(20.f, 30.f, 40.f);
    OriginalContext.AddHitResult(OriginalHit);
    FGameplayEffectContextHandle Duplicate(OriginalContext.Duplicate());
    OriginalHit.ImpactPoint = FVector::ZeroVector;
    OriginalContext.AddHitResult(OriginalHit, true);
    TestEqual(TEXT("Context duplicates HitResult by value"), Duplicate.GetHitResult()->ImpactPoint, FVector(20.f, 30.f, 40.f));
    FCC_DamageRequest Request;
    Request.CriticalPolicy = ECC_CriticalPolicy::Disabled;
    FCC_DamageResolved FirstResult;
    int32 FirstEvents = 0;
    auto* FirstSet = const_cast<UCC_AttributeSet*>(Target->GetSet<UCC_AttributeSet>());
    auto FirstHandle = FirstSet->OnDamageResolved.AddLambda([&](const FCC_DamageResolved& Result) { FirstResult = Result; ++FirstEvents; });
    const auto FirstStatus = UCC_DamageStatics::ApplyDamage(Source, Target, Request);
    FirstSet->OnDamageResolved.Remove(FirstHandle);
    TestTrue(TEXT("Native project cue manager configured"), UAbilitySystemGlobals::Get().GetGameplayCueManager()->IsA<UCC_GameplayCueManager>());
    TestEqual(TEXT("Damage settlement event"), FirstEvents, 1);
    TestTrue(TEXT("Apply accepted"), FirstStatus == ECC_DamageApplyStatus::Applied);
    int32 Controllers = 0;
    for (auto It = World->GetPlayerControllerIterator(); It; ++It) ++Controllers;
    AddInfo(FString::Printf(TEXT("Feedback diagnostics: manager=%s source=%s target=%s controllers=%d local=%d owner=%d bound=%d"),
        *GetNameSafe(UAbilitySystemGlobals::Get().GetGameplayCueManager()->GetClass()), *GetNameSafe(FirstResult.Source), *GetNameSafe(FirstResult.Target),
        Controllers, Viewer->IsLocalController(), Source->GetAvatarActor()->IsOwnedBy(Viewer), Target->OnDamageResolved.IsBound()));
    TestEqual(TEXT("Settlement sends exactly one native Cue entry to attacker"), Text->GetActiveEntries().Num(), 1);
    if (!Text->GetActiveEntries().IsEmpty())
    {
        TestTrue(TEXT("Viewer identity preserved"), Text->GetActiveEntries()[0].Viewer == Viewer);
        TestEqual(TEXT("Resolved amount in Cue"), Text->GetActiveEntries()[0].Amount, 10.f);
    }
    UCC_DamageStatics::ApplyDamage(Source, Target, Request);
    TestEqual(TEXT("Default per-hit display"), Text->GetActiveEntries().Num(), 2);
    Text->Tick(2.f);
    Text->MergeWindow = 0.25f;
    const FVector Location(100.f, 100.f, 100.f);
    AActor* FirstTarget = Target->GetAvatarActor();
    AActor* SecondTarget = World->SpawnActor<AActor>();
    Text->ReportHitForPlayer(Viewer, FirstTarget, Source->GetAvatarActor(), Location, 10.f, ECC_DamageTextStyle::Normal, FGameplayTag());
    Text->ReportHitForPlayer(Viewer, FirstTarget, Source->GetAvatarActor(), Location, 20.f, ECC_DamageTextStyle::Normal, FGameplayTag());
    TestEqual(TEXT("Same target/source/category merges"), Text->GetActiveEntries().Num(), 1);
    TestEqual(TEXT("Merged amount"), Text->GetActiveEntries()[0].Amount, 30.f);
    Text->ReportHitForPlayer(Viewer, SecondTarget, Source->GetAvatarActor(), Location, 5.f, ECC_DamageTextStyle::Normal, FGameplayTag());
    Text->ReportHitForPlayer(Viewer, FirstTarget, Source->GetAvatarActor(), Location, -5.f, ECC_DamageTextStyle::Heal, FGameplayTag());
    Text->ReportHitForPlayer(Viewer, FirstTarget, Source->GetAvatarActor(), Location, 5.f, ECC_DamageTextStyle::Critical, FGameplayTag());
    Text->ReportHitForPlayer(OtherViewer, FirstTarget, Source->GetAvatarActor(), Location, 5.f, ECC_DamageTextStyle::Normal, FGameplayTag());
    TestEqual(TEXT("Targets, healing, critical and split viewers remain independent"), Text->GetActiveEntries().Num(), 5);
    Text->Tick(2.f);

    // Health-change callbacks may synchronously heal; the damage event retains its own before/after.
    Target->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 50.f);
    FCC_DamageResolved DamageResult;
    int32 DamageEvents = 0;
    auto* Set = const_cast<UCC_AttributeSet*>(Target->GetSet<UCC_AttributeSet>());
    const auto ResultHandle = Set->OnDamageResolved.AddLambda([&](const FCC_DamageResolved& Result)
    {
        if (!Result.bHealing) { DamageResult = Result; ++DamageEvents; }
    });
    bool bReentered = false;
    auto& HealthDelegate = Target->GetGameplayAttributeValueChangeDelegate(UCC_AttributeSet::GetHealthAttribute());
    const auto HealthHandle = HealthDelegate.AddLambda([&](const FOnAttributeChangeData& Data)
    {
        if (!bReentered && Data.NewValue < Data.OldValue)
        {
            bReentered = true;
            UCC_DamageStatics::ApplyHealing(Source, Target, 3.f);
        }
    });
    UCC_DamageStatics::ApplyDamage(Source, Target, Request);
    HealthDelegate.Remove(HealthHandle);
    Set->OnDamageResolved.Remove(ResultHandle);
    TestEqual(TEXT("Reentrant heal leaves one damage result"), DamageEvents, 1);
    TestEqual(TEXT("Damage snapshot before reentrant heal"), DamageResult.HealthAfter, 40.f);
    TestEqual(TEXT("Reentrant healing applied independently"), Set->GetHealth(), 43.f);
    TestEqual(TEXT("Reentrancy leaves clean damage meta"), Set->GetIncomingDamage(), 0.f);
    TestEqual(TEXT("Reentrancy leaves clean healing meta"), Set->GetIncomingHealing(), 0.f);
    return true;
}
#endif
