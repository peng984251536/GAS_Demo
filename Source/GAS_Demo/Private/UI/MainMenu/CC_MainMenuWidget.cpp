#include "UI/MainMenu/CC_MainMenuWidget.h"
#include "UI/MainMenu/CC_MainMenuController.h"
#include "CommonButtonBase.h"

// 主菜单只配置自己的控制器和底页关闭规则；房间功能由独立多人页面负责。
UCC_MainMenuWidget::UCC_MainMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_MainMenuController::StaticClass();
	bAllowBack = false;
	bAllowClose = false;
	DefaultFocusWidgetName = TEXT("Button_SinglePlayer");
}

// 搜索属于主菜单意图；控制器负责发起异步请求并打开结果页面。
bool UCC_MainMenuWidget::SearchRooms()
{
	UCC_MainMenuController* Controller = GetMainMenuController();
	FCC_RoomSearchRequest LANRequest = SearchRequest;
	LANRequest.bIsLANQuery = true;
	return Controller && Controller->FindRooms(LANRequest, RoomListScreenClass);
}

// 离房属于主菜单意图，不能用关闭 Widget 或 OpenLevel 代替 Session 清理。
bool UCC_MainMenuWidget::LeaveCurrentRoom()
{
	UCC_MainMenuController* Controller = GetMainMenuController();
	return Controller && Controller->LeaveRoom(FrontEndMap);
}

// 页面基类持有控制器，主菜单不直接访问 OnlineRoomSubsystem。
UCC_MainMenuController* UCC_MainMenuWidget::GetMainMenuController() const
{
	return Cast<UCC_MainMenuController>(GetScreenController());
}

void UCC_MainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Button_SinglePlayer->OnClicked().AddUObject(this, &ThisClass::HandleSinglePlayerClicked);
	Button_HostRoom->OnClicked().AddUObject(this, &ThisClass::HandleHostClicked);
	Button_Multiplayer->OnClicked().AddUObject(this, &ThisClass::HandleMultiplayerClicked);
	Button_LeaveRoom->OnClicked().AddUObject(this, &ThisClass::HandleLeaveClicked);
	Button_Quit->OnClicked().AddUObject(this, &ThisClass::HandleQuitClicked);
	RefreshScreen();
}

void UCC_MainMenuWidget::NativeDestruct()
{
	Button_SinglePlayer->OnClicked().RemoveAll(this);
	Button_HostRoom->OnClicked().RemoveAll(this);
	Button_Multiplayer->OnClicked().RemoveAll(this);
	Button_LeaveRoom->OnClicked().RemoveAll(this);
	Button_Quit->OnClicked().RemoveAll(this);
	Super::NativeDestruct();
}

FCC_MainMenuState UCC_MainMenuWidget::GetMainMenuState() const
{
	const UCC_MainMenuModel* Model = Cast<UCC_MainMenuModel>(GetUIModel());
	return Model ? Model->GetState() : FCC_MainMenuState();
}

void UCC_MainMenuWidget::NativeOnModelChanged()
{
	RefreshScreen();
	Super::NativeOnModelChanged();
}

void UCC_MainMenuWidget::RefreshScreen()
{
	const FCC_MainMenuState State = GetMainMenuState();
	const bool bReady = IsActivated() && !State.bBusy;
	const bool bIdle = State.RoomState == ECC_RoomAsyncState::Idle;
	Button_SinglePlayer->SetIsEnabled(bReady && bIdle);
	Button_HostRoom->SetIsEnabled(bReady && bIdle && State.bRoomServiceAvailable);
	Button_Multiplayer->SetIsEnabled(bReady && bIdle && State.bRoomServiceAvailable);
	Button_LeaveRoom->SetIsEnabled(bReady && State.bRoomServiceAvailable &&
		(State.RoomState == ECC_RoomAsyncState::InRoom || State.RoomState == ECC_RoomAsyncState::RecoveryRequired));
	Button_Quit->SetIsEnabled(bReady);
	OnMainMenuStateChanged(State);
}

bool UCC_MainMenuWidget::StartSinglePlayer()
{
	UCC_MainMenuController* Controller = GetMainMenuController();
	return Controller && Controller->StartLocalGame(SinglePlayerMap);
}

bool UCC_MainMenuWidget::HostLANRoom()
{
	UCC_MainMenuController* Controller = GetMainMenuController();
	return Controller && Controller->HostRoom(HostRequest);
}

bool UCC_MainMenuWidget::OpenSettings()
{
	UCC_MainMenuController* Controller = GetMainMenuController();
	return Controller && Controller->OpenSettings();
}

bool UCC_MainMenuWidget::RequestQuit()
{
	UCC_MainMenuController* Controller = GetMainMenuController();
	return Controller && Controller->RequestQuit();
}

void UCC_MainMenuWidget::HandleSinglePlayerClicked() { StartSinglePlayer(); }
void UCC_MainMenuWidget::HandleHostClicked() { HostLANRoom(); }
void UCC_MainMenuWidget::HandleMultiplayerClicked() { SearchRooms(); }
void UCC_MainMenuWidget::HandleLeaveClicked() { LeaveCurrentRoom(); }
void UCC_MainMenuWidget::HandleQuitClicked() { RequestQuit(); }
