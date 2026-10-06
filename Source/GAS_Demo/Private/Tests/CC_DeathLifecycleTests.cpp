#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/Abilities/Reaction/CC_Death.h"
#include "AbilitySystem/Abilities/Reaction/CC_HitReact.h"
#include "AbilitySystemComponent.h"
#include "Character/CC_EnemyCharacter.h"
#include "Data/CC_CharacterRuntimeData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCDeathLifecycleTest,
	"GAS_Demo.Abilities.Death.DeferredDestroyAndRejectHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCDeathLifecycleTest::RunTest(const FString& Parameters)
{
	const auto Initialization = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("World"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	ACC_EnemyCharacter* Enemy = World->SpawnActor<ACC_EnemyCharacter>();
	if (!TestNotNull(TEXT("Enemy"), Enemy)) return false;
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Enemy, Enemy);
	Enemy->GetRuntimeData()->SetAlive(false);
	const auto HitHandle = ASC->GiveAbility(FGameplayAbilitySpec(UCC_HitReact::StaticClass()));
	ASC->TryActivateAbility(HitHandle);
	TestFalse(TEXT("Direct hit activation on a dead avatar ends before Blueprint execution"),
		ASC->FindAbilitySpecFromHandle(HitHandle)->IsActive());
	TestFalse(TEXT("Dead avatar rejects hit events"), Enemy->GetUCombatActionComponent()->TryGameplayAbilityByTag(
		CCTags::CCAbilityTrigger::BeHit, ASC, ASC));

	Enemy->GetRuntimeData()->SetAlive(true);
	// 无动作数据模拟死亡动画无法播放，仍需安全结束和删除。
	const auto DeathHandle = ASC->GiveAbility(FGameplayAbilitySpec(UCC_Death::StaticClass()));
	TestTrue(TEXT("Death activates"), ASC->TryActivateAbility(DeathHandle));
	TestTrue(TEXT("Enemy remains valid inside the activation call stack"), IsValid(Enemy));
	TestFalse(TEXT("Destroy has not started synchronously"), Enemy->IsActorBeingDestroyed());
	TestTrue(TEXT("Enemy is hidden pending destruction"), Enemy->IsHidden());
	TestFalse(TEXT("Collision is disabled pending destruction"), Enemy->GetActorEnableCollision());
	TestFalse(TEXT("Ability ends before actor destruction"), ASC->FindAbilitySpecFromHandle(DeathHandle)->IsActive());
	World->GetTimerManager().Tick(0.1f);
	TestTrue(TEXT("Enemy is destroyed when the timer runs"), !IsValid(Enemy) || Enemy->IsActorBeingDestroyed());
	return !HasAnyErrors();
}
#endif
