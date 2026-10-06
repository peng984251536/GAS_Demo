#include "UI/MainMenu/CC_MultiplayerScreenController.h"

#include "OnlineRoom/CC_OnlineRoomSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"

UCC_MultiplayerScreenController::UCC_MultiplayerScreenController()
{
	ModelClass = UCC_MultiplayerScreenModel::StaticClass();
}

// 页面每次成为栈顶时重新建立订阅，并读取跨页面保留的房间服务状态。
void UCC_MultiplayerScreenController::OnActivated()
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

// 页面被覆盖或关闭后解除监听，防止不可见页面继续重建列表。
void UCC_MultiplayerScreenController::OnDeactivated()
{
	if (RoomService.IsValid())
	{
		RoomService->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleRoomState);
		RoomService->OnOperationCompleted.RemoveDynamic(this, &ThisClass::HandleRoomResult);
	}
	RoomService.Reset();
	Super::OnDeactivated();
}

// 事件参数用于说明变化原因，展示层始终读取服务的完整权威快照。
void UCC_MultiplayerScreenController::HandleRoomState(ECC_RoomAsyncState NewState)
{
	RefreshState();
}

// 完成事件到达时，服务已经更新 LastResult 和搜索缓存。
void UCC_MultiplayerScreenController::HandleRoomResult(ECC_RoomOperation Operation, FCC_RoomOperationResult Result)
{
	RefreshState();
}

// 模型只保存 UI 需要的数据副本，底层 FOnlineSessionSearchResult 不会泄漏到 Widget。
void UCC_MultiplayerScreenController::RefreshState()
{
	UCC_MultiplayerScreenModel* ScreenModel = Cast<UCC_MultiplayerScreenModel>(GetModel());
	if (!IsActive() || !ScreenModel) return;

	FCC_MultiplayerScreenState State;
	State.ActionError = ActionError;
	State.bRoomServiceAvailable = RoomService.IsValid();
	if (RoomService.IsValid())
	{
		State.bBusy = RoomService->IsBusy();
		State.RoomState = RoomService->GetState();
		State.Rooms = RoomService->GetCachedRooms();
		State.LastRoomResult = RoomService->GetLastResult();
	}
	else
	{
		State.LastRoomResult = FCC_RoomOperationResult::Failure(ECC_RoomResultCode::BackendUnavailable,
			NSLOCTEXT("CCUI", "NoRoomService", "Room service is unavailable."));
	}
	ScreenModel->SetState(State);
}

// 所有房间入口共享同一套交互权校验，防止双击和被覆盖页面发请求。
bool UCC_MultiplayerScreenController::CanUseRoomService()
{
	if (!CanHandleActions()) return false;
	if (!RoomService.IsValid())
	{
		SetActionError(NSLOCTEXT("CCUI", "NoRoomService", "Room service is unavailable."));
		return false;
	}
	if (RoomService->IsBusy()) return false;
	return true;
}

// 页面错误通过模型推送，避免蓝图同时读控制器和模型两套来源。
void UCC_MultiplayerScreenController::SetActionError(const FText& Error)
{
	ActionError = Error;
	RefreshState();
}

// RoomId 只属于最近一轮搜索；底层服务会再次验证它是否仍然有效。
bool UCC_MultiplayerScreenController::JoinRoom(const FString& RoomId)
{
	if (!CanUseRoomService()) return false;
	ActionError = FText::GetEmpty();
	return RoomService->JoinRoom(RoomId);
}

bool UCC_MultiplayerScreenController::RefreshRooms(const FCC_RoomSearchRequest& Request)
{
	if (!CanUseRoomService()) return false;
	ActionError = FText::GetEmpty();
	FCC_RoomSearchRequest LANRequest = Request;
	LANRequest.bIsLANQuery = true;
	return RoomService->FindRooms(LANRequest);
}
