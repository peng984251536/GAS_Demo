#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/Logic/CC_ListenToHealth.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Character/CC_EnemyCharacter.h"
#include "Components/ActionComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCHealthListenerInitializationTest,
	"GAS_Demo.Abilities.HealthListener.ParentInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCHealthListenerInitializationTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
		true, ERHIFeatureLevel::Num, &Initialization);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ACC_EnemyCharacter* Enemy = World->SpawnActor<ACC_EnemyCharacter>();
	if (!TestNotNull(TEXT("Enemy"), Enemy))
	{
		return false;
	}
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Enemy, Enemy);
	ASC->AddAttributeSetSubobject(Enemy->GetAttributeSet());
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 100.f);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UCC_ListenToHealth::StaticClass()));
	UCC_ListenToHealth* Ability = Cast<UCC_ListenToHealth>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	if (!TestNotNull(TEXT("Listener instance"), Ability))
	{
		return false;
	}
	Ability->ShowDebug = false;
	TestTrue(TEXT("Listener activates without action payload"), ASC->TryActivateAbility(Handle));
	TestTrue(TEXT("Listener remains active"), Ability->IsActive());
	FObjectPropertyBase* CharacterProperty = FindFProperty<FObjectPropertyBase>(Ability->GetClass(), TEXT("BaseCharacter"));
	FObjectPropertyBase* ComponentProperty = FindFProperty<FObjectPropertyBase>(Ability->GetClass(), TEXT("CombatComponent"));
	if (TestNotNull(TEXT("Character property"), CharacterProperty)
		&& TestNotNull(TEXT("Component property"), ComponentProperty))
	{
		TestEqual(TEXT("Super initializes character"), CharacterProperty->GetObjectPropertyValue_InContainer(Ability), static_cast<UObject*>(Enemy));
		TestEqual(TEXT("Super initializes component"), ComponentProperty->GetObjectPropertyValue_InContainer(Ability), static_cast<UObject*>(Enemy->GetUCombatActionComponent()));
	}

	// 没有配置死亡动作的原生测试角色：血量归零必须安全返回，不能空指针崩溃。
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 0.f);
	TestTrue(TEXT("Missing death configuration does not break listener"), Ability->IsActive());
	TestFalse(TEXT("Invalid source ASC is rejected safely"),
		Enemy->GetUCombatActionComponent()->TryGameplayAbilityByTag(CCTags::CCAbilityTrigger::Death, nullptr, ASC));
	ASC->CancelAbilityHandle(Handle);
	TestFalse(TEXT("Listener cancels"), Ability->IsActive());
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 50.f);
	TestTrue(TEXT("Listener can reactivate"), ASC->TryActivateAbility(Handle));
	ASC->CancelAbilityHandle(Handle);
	ASC->ClearAbility(Handle);
	return !HasAnyErrors();
}

#endif
