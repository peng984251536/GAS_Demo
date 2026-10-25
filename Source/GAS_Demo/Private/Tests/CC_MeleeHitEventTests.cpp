#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/Mechanics/CC_Primary.h"
#include "Actors/Character/CC_EnemyCharacter.h"
#include "Animation/Notifies/AnimNotify_MeleeHitBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "GameplayTags/CC_Tags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCMeleeHitEventTest, "GAS_Demo.Combat.MeleeHitEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCMeleeHitEventTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptExecution;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto Spawn = [World](const FVector& Location, FGameplayTag Team)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Character = World->SpawnActor<ACC_EnemyCharacter>(Location, FRotator::ZeroRotator, Params);
		Character->GetAbilitySystemComponent()->InitAbilityActorInfo(Character, Character);
		Character->GetRuntimeData()->SetAlive(true);
		Character->SetTeamOverride(Team);
		return Character;
	};
	auto* Attacker = Spawn(FVector::ZeroVector, CCTags::Team::Player);
	auto* Enemy = Spawn(FVector(150, 0, 0), CCTags::Team::Enemy);
	auto* Enemy2 = Spawn(FVector(190, 30, 0), CCTags::Team::Enemy);
	auto* Friendly = Spawn(FVector(140, 0, 0), CCTags::Team::Player);
	auto* Dead = Spawn(FVector(130, -30, 0), CCTags::Team::Enemy);
	Dead->GetRuntimeData()->SetAlive(false);
	Spawn(FVector(1000, 0, 0), CCTags::Team::Enemy);
	auto* Notify = NewObject<UAnimNotify_MeleeHitBox>();
	Notify->Shapes[0].ShapeType = ECC_HitShapeType::Sphere;
	Notify->Shapes[0].Radius = 90.f;
	Notify->Shapes[0].Offset = Attacker->GetMesh()->GetComponentTransform().InverseTransformPosition(FVector(150, 0, 0));
	const FCC_MeleeHitShape DuplicateShape = Notify->Shapes[0];
	Notify->Shapes.Add(DuplicateShape);
	auto* Reader = NewObject<UCC_Primary>();
	FGameplayEventData Received;
	int32 EventCount = 0;
	auto* ASC = Attacker->GetAbilitySystemComponent();
	const FGameplayTagContainer Tags(CCTags::Event::Attack.GetTag());
	const FDelegateHandle Listener = ASC->AddGameplayEventTagContainerDelegate(Tags,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateLambda([&](FGameplayTag, const FGameplayEventData* Event)
		{
			++EventCount;
			Received = *Event;
		}));
	Notify->Notify(Attacker->GetMesh(), nullptr, FAnimNotifyEventReference());
	TestEqual(TEXT("One event per notify despite multiple shapes"), EventCount, 1);
	TestTrue(TEXT("Event instigator is attacker"), Received.Instigator.Get() == Attacker);
	TestTrue(TEXT("Event sent to attacker, not victims"), Received.Target.Get() == Attacker);
	TestTrue(TEXT("Notify identity preserved"), UAnimNotify_MeleeHitBox::FromEventPayload(Received) == Notify);
	const auto Hits = Reader->HitBoxOverlapFromEvent(Received);
	TestEqual(TEXT("Two hostile actors, deduplicated across shapes/components"), Hits.Num(), 2);
	TestTrue(TEXT("First enemy collected"), Hits.ContainsByPredicate([&](const auto& H) { return H.hitActor == Enemy; }));
	TestTrue(TEXT("Second enemy collected"), Hits.ContainsByPredicate([&](const auto& H) { return H.hitActor == Enemy2; }));
	TestFalse(TEXT("Friendly omitted"), Hits.ContainsByPredicate([&](const auto& H) { return H.hitActor == Friendly; }));
	for (const auto& Hit : Hits)
	{
		TestTrue(TEXT("HitResult retains actor/component for downstream damage"), Hit.Result.GetActor() == Hit.hitActor && Hit.Result.GetComponent());
		TestFalse(TEXT("Surface normal is nonzero"), Hit.Result.ImpactNormal.IsNearlyZero());
	}
	// 已发送的数据必须是快照：修改通知/移动目标后，能力不能重新检测。
	Enemy->SetActorLocation(FVector(2000, 0, 0));
	Enemy2->SetActorLocation(FVector(2500, 0, 0));
	TestEqual(TEXT("Reader uses event snapshot, does not query scene again"), Reader->HitBoxOverlapFromEvent(Received).Num(), 2);
	Notify->Notify(Attacker->GetMesh(), nullptr, FAnimNotifyEventReference());
	TestEqual(TEXT("Miss still sends timing event"), EventCount, 2);
	TestEqual(TEXT("Miss does not reuse previous hits or fall back to old sphere"), Reader->HitBoxOverlapFromEvent(Received).Num(), 0);
	FGameplayEventData Unrelated;
	Unrelated.TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(FHitResult(Enemy, nullptr, FVector::ZeroVector, FVector::UpVector)));
	TestEqual(TEXT("Unrelated events rejected"), Reader->HitBoxOverlapFromEvent(Unrelated).Num(), 0);
	// 盒子、胶囊与球都使用 Notify 形状本身，而不是能力固定球形范围。
	Enemy->SetActorLocation(FVector(150, 0, 0));
	Notify->Shapes.SetNum(1);
	Notify->Shapes[0].ShapeType = ECC_HitShapeType::Box;
	Notify->Shapes[0].BoxHalfExtent = FVector(20, 20, 40);
	TestEqual(TEXT("Box hit"), Notify->CollectTargetData(Attacker->GetMesh()).Num(), 1);
	Notify->Shapes[0].ShapeType = ECC_HitShapeType::Capsule;
	Notify->Shapes[0].Radius = 30.f;
	Notify->Shapes[0].CapsuleHalfHeight = 70.f;
	TestEqual(TEXT("Capsule hit"), Notify->CollectTargetData(Attacker->GetMesh()).Num(), 1);
	ASC->RemoveGameplayEventTagContainerDelegate(Tags, Listener);
	return true;
}
#endif
