#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UI/HealthBar/CC_BatchedHealthBarSubsystem.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Character/CC_EnemyCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "CommonGameViewportClient.h"

/** 验证数据管理契约：去重、初值、插值、销毁清理，以及 GAS 当前值和最大值的事件更新。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBatchedHealthBarTest,
	"GASDemo.UI.HealthBars.RegistrationAndGAS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCBatchedHealthBarTest::RunTest(const FString& Parameters)
{
	// 构建独立游戏世界和本地玩家，不依赖当前关卡，不需要可见窗口。
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
	// 即使断言提前返回，也释放本地玩家、游戏实例、视口与世界，避免污染后续测试。
	ON_SCOPE_EXIT
	{
		Instance->RemoveLocalPlayer(Player);
		Instance->Shutdown();
		Viewport->DetachViewportClient();
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	UCC_BatchedHealthBarSubsystem* Manager = Player->GetSubsystem<UCC_BatchedHealthBarSubsystem>();
	if (!TestNotNull(TEXT("Local player manager"), Manager)) return false;
	TestFalse(TEXT("Idle manager does not tick"), Manager->IsTickable());
	// 手动数据模式：同一 Actor 重复注册必须去重，初次推送直接定位，后续变化平滑过渡。
	AActor* ManualActor = World->SpawnActor<AActor>();
	FCC_HealthBarOptions Options;
	Options.bUseGAS = false;
	TestTrue(TEXT("Manual registration"), Manager->RegisterHealthBar(ManualActor, Options));
	TestTrue(TEXT("Duplicate registration"), Manager->RegisterHealthBar(ManualActor, Options));
	TestEqual(TEXT("No duplicate entry"), Manager->GetRegisteredCount(), 1);
	TestTrue(TEXT("Manual update"), Manager->UpdateHealthBar(ManualActor, 25, 100));
	TestEqual(TEXT("Initial fraction"), Manager->GetEntries()[0].DisplayedFraction, 0.25f);
	Manager->UpdateHealthBar(ManualActor, 50, 100);
	Manager->Tick(0.02f);
	TestTrue(TEXT("Fraction interpolates"), Manager->GetEntries()[0].DisplayedFraction > 0.25f && Manager->GetEntries()[0].DisplayedFraction < 0.5f);
	Manager->SetHealthBarVisible(ManualActor, false);
	TestFalse(TEXT("Visibility flag"), Manager->GetEntries()[0].bVisible);
	// 最大生命值为 0 和角色销毁是常见边界，分别验证除零保护和自动移除。
	Manager->UpdateHealthBar(ManualActor, 50, 0);
	Manager->Tick(1.f);
	TestEqual(TEXT("Zero max health is safe"), Manager->GetEntries()[0].DisplayedFraction, 0.f);
	ManualActor->Destroy();
	Manager->Tick(0.01f);
	TestEqual(TEXT("Destroyed actor removed"), Manager->GetRegisteredCount(), 0);

	// GAS 模式：显式初始化原生敌人的 ASC / 属性集，检查两个属性的委托都能即时推送。
	ACC_EnemyCharacter* Enemy = World->SpawnActor<ACC_EnemyCharacter>();
	if (!TestNotNull(TEXT("GAS actor"), Enemy)) return false;
	UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Enemy, Enemy);
	ASC->AddAttributeSetSubobject(Enemy->GetAttributeSet());
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 200);
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 100);
	Options.bUseGAS = true;
	TestTrue(TEXT("GAS registration"), Manager->RegisterHealthBar(Enemy, Options));
	TestEqual(TEXT("Initial GAS health"), Manager->GetEntries()[0].Health, 100.f);
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 75);
	TestEqual(TEXT("Health delegate updates synchronously"), Manager->GetEntries()[0].Health, 75.f);
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetMaxHealthAttribute(), 300);
	TestEqual(TEXT("Max health delegate updates synchronously"), Manager->GetEntries()[0].MaxHealth, 300.f);
	TestFalse(TEXT("Manual update cannot override GAS"), Manager->UpdateHealthBar(Enemy, 1, 2));
	Manager->RegisterHealthBar(Enemy, Options);
	// 移除后继续修改属性，确认旧回调不会重新添加条目，清空后应恢复空闲。
	Manager->UnregisterHealthBar(Enemy);
	ASC->SetNumericAttributeBase(UCC_AttributeSet::GetHealthAttribute(), 50);
	TestEqual(TEXT("Unregistered actor stays removed after attribute change"), Manager->GetRegisteredCount(), 0);
	Manager->RegisterHealthBar(Enemy, Options);
	Manager->ClearHealthBars();
	TestFalse(TEXT("Clear returns to idle"), Manager->IsTickable());
	return !HasAnyErrors();
}
#endif
