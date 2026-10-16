#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "CommonGameViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

/** 子系统增删契约与锚点计算；显示层需要视口，在 PIE 中验证。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCWorldIndicatorSubsystemTest, "GAS_Demo.UI.WorldIndicator.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCWorldIndicatorSubsystemTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("World"), World)) return false;
	FWorldContext& Context = GEngine->GetWorldContextFromWorldChecked(World);
	UCommonGameViewportClient* Viewport = NewObject<UCommonGameViewportClient>(GEngine);
	Context.GameViewport = Viewport;
	Viewport->Init(Context, Instance, false);
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	Instance->AddLocalPlayer(Player, FPlatformUserId::CreateFromInternalId(0));
	ON_SCOPE_EXIT
	{
		Instance->RemoveLocalPlayer(Player);
		Instance->Shutdown();
		Viewport->DetachViewportClient();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	UCC_WorldIndicatorSubsystem* Subsystem = Player->GetSubsystem<UCC_WorldIndicatorSubsystem>();
	if (!TestNotNull(TEXT("Local player subsystem"), Subsystem)) return false;
	int32 Added = 0, Removed = 0;
	Subsystem->OnIndicatorAdded.AddLambda([&](UCC_WorldIndicator*) { ++Added; });
	Subsystem->OnIndicatorRemoved.AddLambda([&](UCC_WorldIndicator*) { ++Removed; });

	// 固定坐标
	UCC_WorldIndicator* Fixed = NewObject<UCC_WorldIndicator>(Subsystem);
	Fixed->WorldLocation = FVector(100.f, 0.f, 0.f);
	Fixed->WorldOffset = FVector(0.f, 0.f, 50.f);
	FVector Location;
	TestTrue(TEXT("Fixed anchor resolves"), Fixed->GetAnchorLocation(Location));
	TestEqual(TEXT("Fixed anchor includes offset"), Location, FVector(100.f, 0.f, 50.f));
	Subsystem->AddIndicator(Fixed);
	Subsystem->AddIndicator(Fixed);
	TestEqual(TEXT("Duplicate add ignored"), Subsystem->GetIndicators().Num(), 1);
	TestEqual(TEXT("One add event"), Added, 1);

	// 跟随 Actor：Actor 销毁后锚点失效
	AActor* Actor = World->SpawnActor<AActor>();
	UCC_WorldIndicator* Follow = NewObject<UCC_WorldIndicator>(Subsystem);
	Follow->SetAnchorActor(Actor);
	Follow->WorldOffset = FVector(0.f, 0.f, 80.f);
	Subsystem->AddIndicator(Follow);
	TestTrue(TEXT("Follow anchor resolves"), Follow->GetAnchorLocation(Location));
	TestEqual(TEXT("Follow anchor tracks actor"), Location, Actor->GetActorLocation() + FVector(0.f, 0.f, 80.f));
	Actor->Destroy();
	TestFalse(TEXT("Destroyed anchor does not resolve"), Follow->GetAnchorLocation(Location));
	TestTrue(TEXT("Destroyed anchor is reported lost"), Follow->IsAnchorLost());
	TestFalse(TEXT("Fixed anchor is never lost"), Fixed->IsAnchorLost());

	Subsystem->RemoveIndicator(Follow);
	Subsystem->RemoveIndicator(Follow);
	TestEqual(TEXT("Remove is idempotent"), Removed, 1);
	Subsystem->ClearIndicators();
	TestEqual(TEXT("Clear removes the rest"), Subsystem->GetIndicators().Num(), 0);
	TestEqual(TEXT("Clear notifies each indicator"), Removed, 2);
	Subsystem->OnIndicatorAdded.Clear();
	Subsystem->OnIndicatorRemoved.Clear();
	return true;
}

#endif
