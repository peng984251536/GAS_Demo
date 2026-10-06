#include "UI/MainMenu/CC_MultiplayerRoomItemWidget.h"

#include "CommonButtonBase.h"
#include "UI/MainMenu/CC_MultiplayerScreenWidget.h"

void UCC_MultiplayerRoomEntry::Initialize(
	const FCC_RoomSummary& InRoom, UCC_MultiplayerScreenWidget* InOwner, bool bInCanJoin)
{
	Room = InRoom;
	OwnerScreen = InOwner;
	bCanJoin = bInCanJoin;
}

bool UCC_MultiplayerRoomEntry::RequestJoin()
{
	return bCanJoin && OwnerScreen && OwnerScreen->JoinRoom(Room.RoomId);
}

void UCC_MultiplayerRoomItemWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Button_JoinRoom->OnClicked().AddUObject(this, &ThisClass::HandleJoinClicked);
}

void UCC_MultiplayerRoomItemWidget::NativeDestruct()
{
	Button_JoinRoom->OnClicked().RemoveAll(this);
	RoomEntry = nullptr;
	Super::NativeDestruct();
}

void UCC_MultiplayerRoomItemWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	RoomEntry = Cast<UCC_MultiplayerRoomEntry>(ListItemObject);
	const bool bCanJoin = RoomEntry && RoomEntry->CanJoin();
	Button_JoinRoom->SetIsEnabled(bCanJoin);
	OnRoomItemChanged(RoomEntry ? RoomEntry->GetRoom() : FCC_RoomSummary(), bCanJoin);
}

void UCC_MultiplayerRoomItemWidget::HandleJoinClicked()
{
	if (RoomEntry) RoomEntry->RequestJoin();
}
