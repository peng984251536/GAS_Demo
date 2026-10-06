#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameplayTags/CC_Tags.h"
#include "UI/Framework/CC_DemoScreens.h"
#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_AsyncShowScreen.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "CommonGameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

/** 验证独立控制器的会话隔离，避免迟到回调更新重新激活的页面。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCUIControllerLifecycleTest,
	"GASDemo.UI.Controller.LifecycleIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCUIControllerLifecycleTest::RunTest(const FString& Parameters)
{
	// 此测试只验证 UObject 生命周期契约，不构建 Slate 或调用游戏业务。
	UCC_PauseMenuWidget* View = NewObject<UCC_PauseMenuWidget>();
	UCC_UIController* Controller = NewObject<UCC_UIController>(View);
	Controller->Initialize(View, nullptr);
	UCC_UIModel* OriginalModel = Controller->GetModel();
	TestNotNull(TEXT("Initialization creates the model"), OriginalModel);
	Controller->Activate();
	const FGuid FirstToken = Controller->GetActivationToken();
	TestTrue(TEXT("Active session accepts its token"), Controller->IsActivationCurrent(FirstToken));
	TestFalse(TEXT("No root layout means no user actions"), Controller->CanHandleActions());
	Controller->Activate();
	TestTrue(TEXT("Repeated activation is idempotent"), Controller->GetActivationToken() == FirstToken);
	Controller->Deactivate();
	Controller->Deactivate();
	TestFalse(TEXT("Deactivation rejects late callbacks"), Controller->IsActivationCurrent(FirstToken));
	Controller->Activate();
	TestFalse(TEXT("Reactivation never accepts the old token"), Controller->IsActivationCurrent(FirstToken));
	TestTrue(TEXT("Covered page retains its model"), Controller->GetModel() == OriginalModel);
	Controller->Release();
	Controller->Activate();
	TestFalse(TEXT("Released controller cannot reactivate without an owner"), Controller->IsActive());

	UCC_UIController* ReopenedController = NewObject<UCC_UIController>(View);
	ReopenedController->Initialize(View, nullptr);
	ReopenedController->Activate();
	TestTrue(TEXT("New opening has an independent model"), ReopenedController->GetModel() != OriginalModel);
	TestFalse(TEXT("New opening rejects the previous controller token"), ReopenedController->IsActivationCurrent(FirstToken));
	ReopenedController->Release();

	// 同层同类页面被再次 ShowScreen：注入新 Context 时重启激活会话，旧异步回调失效，模型保留。
	UCC_UIController* ReusedController = NewObject<UCC_UIController>(View);
	ReusedController->Initialize(View, nullptr);
	ReusedController->Activate();
	const FGuid BeforeReuseToken = ReusedController->GetActivationToken();
	UCC_UIModel* ReusedModel = ReusedController->GetModel();
	ReusedController->UpdateContext(NewObject<UCC_UIModel>());
	TestTrue(TEXT("Context update keeps the page active"), ReusedController->IsActive());
	TestFalse(TEXT("Context update invalidates the previous session token"), ReusedController->IsActivationCurrent(BeforeReuseToken));
	TestTrue(TEXT("Context update keeps the same model"), ReusedController->GetModel() == ReusedModel);
	ReusedController->Deactivate();
	ReusedController->UpdateContext(NewObject<UCC_UIModel>());
	TestFalse(TEXT("Covered page is not activated by a context update"), ReusedController->IsActive());
	ReusedController->Release();

	// 模拟同一池化 Widget 重新入栈，验证无需等待 NativeDestruct 才清理旧会话。
	View->PrepareForDisplay(nullptr);
	UCC_UIController* PreviousController = View->GetScreenController();
	if (!TestNotNull(TEXT("Page creates its configured controller"), PreviousController)) return false;
	PreviousController->Activate();
	const FGuid PreviousToken = PreviousController->GetActivationToken();
	View->PrepareForDisplay(nullptr);
	TestTrue(TEXT("Pooled page receives a fresh controller"), View->GetScreenController() != PreviousController);
	TestFalse(TEXT("Pooled reopening invalidates old callbacks immediately"), PreviousController->IsActivationCurrent(PreviousToken));
	View->GetScreenController()->Release();
	return true;
}

/** 验证学习版四层隔离、并发加载令牌以及根布局结束时取消异步请求。无需实际窗口。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCUIRootLifecycleTest,
	"GASDemo.UI.Root.LayersAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCUIRootLifecycleTest::RunTest(const FString& Parameters)
{
	UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	UCommonGameViewportClient* Viewport = NewObject<UCommonGameViewportClient>(GEngine);
	WorldContext.GameViewport = Viewport;
	Viewport->Init(WorldContext, Instance, false);
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
	APlayerController* PC = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Test controller"), PC)) return false;
	// Standalone 的占位世界尚未执行 Actor 初始化，需要显式登记 Controller。
	// FLocalPlayerContext 通过世界的 Controller 列表查找玩家，而非只读 PlayerController 指针。
	World->AddController(PC);
	PC->SetPlayer(Player);
	if (!TestTrue(TEXT("Local player context is valid"), FLocalPlayerContext(Player, World).IsValid())) return false;
	UCC_RootLayout* Root = CreateWidget<UCC_RootLayout>(PC);
	if (!TestNotNull(TEXT("Root"), Root)) return false;
	Root->ResumeLayout(); // 此独立根用于无窗口容器测试，显式模拟 Policy 完成挂载。
	const FGameplayTag Game = CCTags::UILayer::Game;
	const FGameplayTag GameMenu = CCTags::UILayer::GameMenu;
	const FGameplayTag Menu = CCTags::UILayer::Menu;
	const FGameplayTag Modal = CCTags::UILayer::Modal;
	TestNotNull(TEXT("Game layer"), Root->GetLayer(Game));
	TestNotNull(TEXT("GameMenu layer"), Root->GetLayer(GameMenu));
	TestNotNull(TEXT("Menu layer"), Root->GetLayer(Menu));
	TestNotNull(TEXT("Modal layer"), Root->GetLayer(Modal));
	TestTrue(TEXT("GameMenu is independent from Menu"), Root->GetLayer(GameMenu) != Root->GetLayer(Menu));
	TestNull(TEXT("Unknown layer rejected"), Root->GetLayer(FGameplayTag()));
	const FName First = Root->SuspendAsyncInput();
	const FName Second = Root->SuspendAsyncInput();
	Root->ResumeAsyncInput(First);
	TestTrue(TEXT("Second request keeps input blocked"), Root->IsInputBlocked());
	Root->ResumeAsyncInput(Second);
	TestFalse(TEXT("All requests finished unlock input"), Root->IsInputBlocked());
	// 脱离视口和永久销毁是不同状态：清空后保留容器对象，重新挂载可以继续导航。
	UCommonActivatableWidgetContainerBase* OriginalMenu = Root->GetLayer(Menu);
	Root->ResetWorldContent();
	TestFalse(TEXT("Detached root rejects navigation"), Root->CanAcceptScreen());
	TestFalse(TEXT("World cleanup is not final shutdown"), Root->IsShuttingDown());
	Root->ResumeLayout();
	TestTrue(TEXT("Reattached root accepts navigation"), Root->CanAcceptScreen());
	TestTrue(TEXT("Container identity survives world reset"), Root->GetLayer(Menu) == OriginalMenu);
	// 标准 APlayerController 足以触发 Manager；不依赖已删除的 UI 控制器父类。
	UCC_UIManagerSubsystem* Manager = Instance->GetSubsystem<UCC_UIManagerSubsystem>();
	UCC_RootLayout* ManagedRoot = Manager ? Manager->GetRootLayout(Player) : nullptr;
	TestNotNull(TEXT("Engine player event creates root without project controller"), ManagedRoot);
	if (ManagedRoot)
	{
		FWorldDelegates::OnWorldBeginTearDown.Broadcast(World);
		TestTrue(TEXT("World teardown preserves root identity"), Manager->GetRootLayout(Player) == ManagedRoot);
		TestFalse(TEXT("World teardown detaches root"), ManagedRoot->IsLayoutAttached());
		Player->ReceivedPlayerController(PC);
		TestTrue(TEXT("Controller notification reuses root"), Manager->GetRootLayout(Player) == ManagedRoot);
		TestTrue(TEXT("Controller notification reattaches root"), ManagedRoot->IsLayoutAttached());
	}
	UCC_AsyncShowScreen* Action = UCC_AsyncShowScreen::ShowScreenAsync(Root, Menu,
		TSoftClassPtr<UCC_ActivatableWidget>(UCC_PauseMenuWidget::StaticClass()), nullptr);
	Action->Activate();
	TestTrue(TEXT("Async loading holds input"), Root->IsInputBlocked());
	Root->Shutdown();
	TestFalse(TEXT("Shutdown cancels pending action"), Action->IsActive());
	TestFalse(TEXT("Shutdown releases loading input"), Root->IsInputBlocked());
	TestFalse(TEXT("Shutdown rejects later navigation"), Root->CanAcceptScreen());
	Root->Shutdown();
	TestNull(TEXT("Closed root refuses screens"), Root->ShowScreen(Menu, UCC_PauseMenuWidget::StaticClass()));
	return !HasAnyErrors();
}

#endif
