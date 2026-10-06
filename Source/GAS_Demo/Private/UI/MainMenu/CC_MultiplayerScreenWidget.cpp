#include "UI/MainMenu/CC_MultiplayerScreenWidget.h"

#include "CommonButtonBase.h"
#include "Components/ListView.h"
#include "UI/MainMenu/CC_MultiplayerRoomItemWidget.h"

UCC_MultiplayerScreenWidget::UCC_MultiplayerScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ControllerClass = UCC_MultiplayerScreenController::StaticClass();
	bAllowBack = true;
	DefaultFocusWidgetName = TEXT("Button_Refresh");
}

void UCC_MultiplayerScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Button_Refresh->OnClicked().AddUObject(this, &ThisClass::HandleRefreshClicked);
	Button_Back->OnClicked().AddUObject(this, &ThisClass::HandleBackClicked);
	RefreshScreen();
}

void UCC_MultiplayerScreenWidget::NativeDestruct()
{
	Button_Refresh->OnClicked().RemoveAll(this);
	Button_Back->OnClicked().RemoveAll(this);
	List_Rooms->ClearListItems();
	RoomEntries.Reset();
	Super::NativeDestruct();
}

void UCC_MultiplayerScreenWidget::NativeOnModelChanged()
{
	RefreshScreen();
	Super::NativeOnModelChanged();
}

UCC_MultiplayerScreenController* UCC_MultiplayerScreenWidget::GetMultiplayerController() const
{
	return Cast<UCC_MultiplayerScreenController>(GetScreenController());
}

FCC_MultiplayerScreenState UCC_MultiplayerScreenWidget::GetMultiplayerState() const
{
	const UCC_MultiplayerScreenModel* ScreenModel = Cast<UCC_MultiplayerScreenModel>(GetUIModel());
	return ScreenModel ? ScreenModel->GetState() : FCC_MultiplayerScreenState();
}

void UCC_MultiplayerScreenWidget::RefreshScreen()
{
	FCC_MultiplayerScreenState State = GetMultiplayerState();
	if (State.RoomState == ECC_RoomAsyncState::Searching) State.Rooms.Reset();

	const bool bCanInteract = IsActivated() && State.bRoomServiceAvailable && !State.bBusy;
	const bool bCanJoin = bCanInteract && State.RoomState == ECC_RoomAsyncState::Idle;
	Button_Refresh->SetIsEnabled(bCanJoin);
	Button_Back->SetIsEnabled(IsActivated());

	List_Rooms->ClearListItems();
	RoomEntries.Reset(State.Rooms.Num());
	for (const FCC_RoomSummary& Room : State.Rooms)
	{
		UCC_MultiplayerRoomEntry* Entry = NewObject<UCC_MultiplayerRoomEntry>(this);
		Entry->Initialize(Room, this, bCanJoin && Room.bJoinable);
		RoomEntries.Add(Entry);
		List_Rooms->AddItem(Entry);
	}
	OnMultiplayerStateChanged(State);
}

bool UCC_MultiplayerScreenWidget::RefreshRooms()
{
	UCC_MultiplayerScreenController* Controller = GetMultiplayerController();
	return Controller && Controller->RefreshRooms(SearchRequest);
}

bool UCC_MultiplayerScreenWidget::JoinRoom(const FString& RoomId)
{
	const FCC_MultiplayerScreenState State = GetMultiplayerState();
	if (RoomId.IsEmpty() || State.bBusy || State.RoomState != ECC_RoomAsyncState::Idle) return false;
	if (!State.Rooms.ContainsByPredicate([&RoomId](const FCC_RoomSummary& Room)
		{ return Room.RoomId == RoomId && Room.bJoinable; })) return false;
	UCC_MultiplayerScreenController* Controller = GetMultiplayerController();
	return Controller && Controller->JoinRoom(RoomId);
}

bool UCC_MultiplayerScreenWidget::GoBack()
{
	UCC_MultiplayerScreenController* Controller = GetMultiplayerController();
	return Controller && Controller->RequestClose();
}

void UCC_MultiplayerScreenWidget::HandleRefreshClicked() { RefreshRooms(); }
void UCC_MultiplayerScreenWidget::HandleBackClicked() { GoBack(); }
