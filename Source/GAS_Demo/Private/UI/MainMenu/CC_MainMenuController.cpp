#include "UI/MainMenu/CC_MainMenuController.h"
#include "GameplayTags/CC_Tags.h"

#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_DemoScreens.h"
#include "OnlineRoom/CC_OnlineRoomSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

UCC_MainMenuController::UCC_MainMenuController()
{
	ModelClass = UCC_MainMenuModel::StaticClass();
	QuitScreenClass = UCC_QuitDialogWidget::StaticClass();
}

// 激活主菜单时发布一次初始展示状态。
void UCC_MainMenuController::OnActivated()
{
	APlayerController* Player = GetPlayerController();
	UGameInstance* Game = Player ? Player->GetGameInstance() : nullptr;
	RoomService = Game ? Game->GetSubsystem<UCC_OnlineRoomSubsystem>() : nullptr;
	if (RoomService.IsValid())
	{
		RoomService->OnStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoomState);
		RoomService->OnOperationCompleted.AddUniqueDynamic(this, &ThisClass::HandleRoomResult);
	}
	RefreshState();
	Super::OnActivated();
}

// 页面关闭后作废其异步开始请求，迟到回调将无法更新新页面。
void UCC_MainMenuController::OnDeactivated()
{
	StartRequest.Invalidate();
	StartActivation.Invalidate();
	if (RoomService.IsValid())
	{
		RoomService->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleRoomState);
		RoomService->OnOperationCompleted.RemoveDynamic(this, &ThisClass::HandleRoomResult);
	}
	RoomService.Reset();
	Super::OnDeactivated();
}

// 主菜单不保存房间数据，只在状态变化时读取房间服务的最新快照。
void UCC_MainMenuController::HandleRoomState(ECC_RoomAsyncState NewState) { RefreshState(); }
// 搜索和离房的完成结果由统一模型推给主菜单。
void UCC_MainMenuController::HandleRoomResult(ECC_RoomOperation Operation, FCC_RoomOperationResult Result) { RefreshState(); }

// 将控制器内部状态作为一个完整快照提交给主菜单模型。
void UCC_MainMenuController::RefreshState()
{
	UCC_MainMenuModel* MenuModel = Cast<UCC_MainMenuModel>(GetModel());
	if (!IsActive() || !MenuModel) return;
	FCC_MainMenuState State;
	State.bRoomServiceAvailable = RoomService.IsValid();
	State.bBusy = StartRequest.IsValid() || (RoomService.IsValid() && RoomService->IsBusy());
	if (RoomService.IsValid())
	{
		State.RoomState = RoomService->GetState();
		State.LastRoomResult = RoomService->GetLastResult();
	}
	State.ActionError = ActionError;
	MenuModel->SetState(State);
}

// CommonUI 栈顶、过渡状态与页面请求三项同时控制交互权。
bool UCC_MainMenuController::CanStartAction() const
{
	return CanHandleActions() && !StartRequest.IsValid() && (!RoomService.IsValid() || !RoomService->IsBusy());
}

// 搜索和离开必须从可交互的主菜单发起，并使用同一个房间门面防止事务重入。
bool UCC_MainMenuController::CanUseRoomService()
{
	if (!CanStartAction()) return false;
	if (!RoomService.IsValid())
	{
		SetActionError(NSLOCTEXT("CCUI", "NoRoomService", "Room service is unavailable."));
		return false;
	}
	return true;
}

// 错误由模型统一推送，Widget 不直接读取控制器成员。
void UCC_MainMenuController::SetActionError(const FText& Error)
{
	ActionError = Error;
	RefreshState();
}

// 先发起异步搜索，再打开房间列表页；列表页会监听同一个 GameInstance 子系统并接收结果。
bool UCC_MainMenuController::FindRooms(
	const FCC_RoomSearchRequest& Request, TSubclassOf<UCC_ActivatableWidget> RoomListScreenClass)
{
	if (!CanUseRoomService()) return false;
	if (!RoomListScreenClass || RoomListScreenClass->HasAnyClassFlags(CLASS_Abstract))
	{
		SetActionError(NSLOCTEXT("CCUI", "MissingRoomList", "Configure a multiplayer Widget Blueprint in Room List Screen Class."));
		return false;
	}
	ActionError = FText::GetEmpty();
	// Keep the service locally: showing the next page deactivates this controller.
	UCC_OnlineRoomSubsystem* Service = RoomService.Get();
	UCC_RootLayout* Root = GetRootLayout();
	if (Root && Root->ShowScreen(CCTags::UILayer::Menu, RoomListScreenClass))
		return Service->FindRooms(Request);
	SetActionError(NSLOCTEXT("CCUI", "RoomListUnavailable", "The room list screen could not be opened."));
	return false;
}

bool UCC_MainMenuController::HostRoom(const FCC_RoomCreateRequest& Request)
{
	if (!CanUseRoomService()) return false;
	ActionError = FText::GetEmpty();
	FCC_RoomCreateRequest LANRequest = Request;
	LANRequest.bIsLANMatch = true;
	return RoomService->HostRoom(LANRequest);
}

bool UCC_MainMenuController::StartLocalGame(TSoftObjectPtr<UWorld> Map)
{
	if (!CanStartAction()) return false;
	if (RoomService.IsValid() && RoomService->GetState() != ECC_RoomAsyncState::Idle)
	{
		SetActionError(NSLOCTEXT("CCUI", "SessionMustClose", "Leave or clean up the current room before starting a local game."));
		return false;
	}
	const FString Package = Map.ToSoftObjectPath().GetLongPackageName();
	if (!GetWorld() || !FPackageName::DoesPackageExist(Package))
	{
		SetActionError(NSLOCTEXT("CCUI", "MissingSingleMap", "Configure an existing Single Player Map."));
		return false;
	}
	ActionError = FText::GetEmpty();
	UGameplayStatics::OpenLevel(this, FName(*Package), true);
	return true;
}

// 离开按钮只负责 Session 清理；普通页面返回仍使用 CloseScreen。
bool UCC_MainMenuController::LeaveRoom(TSoftObjectPtr<UWorld> FrontEndMap)
{
	if (!CanUseRoomService()) return false;
	ActionError = FText::GetEmpty();
	return RoomService->LeaveRoom(FrontEndMap);
}

// 设置页仍由主菜单负责导航，与房间列表职责无关。
bool UCC_MainMenuController::OpenSettings()
{
	if (!CanStartAction()) return false;
	if (UCC_RootLayout* Root = GetRootLayout())
	{
		if (Root->ShowScreen(CCTags::UILayer::Menu, SettingsScreenClass)) return true;
	}
	SetActionError(NSLOCTEXT("CCUI", "SettingsUnavailable", "The settings screen could not be opened. Check its configured class."));
	return false;
}

// 退出应用程序先打开 Modal 确认框，避免主菜单按钮直接终止进程。
bool UCC_MainMenuController::RequestQuit()
{
	if (!CanStartAction()) return false;
	if (UCC_RootLayout* Root = GetRootLayout())
	{
		if (Root->ShowScreen(CCTags::UILayer::Modal, QuitScreenClass)) return true;
	}
	SetActionError(NSLOCTEXT("CCUI", "QuitUnavailable", "The quit confirmation screen could not be opened."));
	return false;
}

// 本地开始前只做会话残留保护；加入操作由房间列表页负责。
bool UCC_MainMenuController::RequestStartGame(bool bContinue)
{
	if (!CanStartAction()) return false;
	if (RoomService.IsValid() && RoomService->GetState() != ECC_RoomAsyncState::Idle)
	{
		SetActionError(NSLOCTEXT("CCUI", "SessionMustClose", "Leave or clean up the current room before starting a local game."));
		return false;
	}
	StartRequest = FGuid::NewGuid();
	StartActivation = GetActivationToken();
	const FGuid Request = StartRequest;
	SetActionError(FText::GetEmpty());
	if (!IsActivationCurrent(StartActivation) || StartRequest != Request) return false;
	ExecuteStartGame(Request, bContinue);
	return true;
}

// 只接受当前激活会话的当前请求，避免异步完成污染重新打开的主菜单。
bool UCC_MainMenuController::CompleteStartGame(FGuid RequestId, bool bSuccess, FText Error)
{
	if (!RequestId.IsValid() || RequestId != StartRequest || !IsActivationCurrent(StartActivation)) return false;
	StartRequest.Invalidate();
	StartActivation.Invalidate();
	SetActionError(bSuccess ? FText::GetEmpty() : Error);
	return true;
}

// 项目未接入存档服务时立即失败，让界面明确显示缺少的配置。
void UCC_MainMenuController::ExecuteStartGame_Implementation(FGuid RequestId, bool bContinue)
{
	CompleteStartGame(RequestId, false,
		NSLOCTEXT("CCUI", "StartNotConfigured", "Implement ExecuteStartGame in the menu controller to connect your save/load service."));
}
