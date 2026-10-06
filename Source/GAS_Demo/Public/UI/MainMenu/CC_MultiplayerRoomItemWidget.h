#pragma once

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "OnlineRoom/CC_RoomTypes.h"
#include "CC_MultiplayerRoomItemWidget.generated.h"

class UCommonButtonBase;
class UCC_MultiplayerScreenWidget;

/** ListView 数据对象：保存一条房间快照，并把加入请求送回所属多人页。 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_MultiplayerRoomEntry : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="UI|Multiplayer") FCC_RoomSummary GetRoom() const { return Room; }
	UFUNCTION(BlueprintPure, Category="UI|Multiplayer") bool CanJoin() const { return bCanJoin; }
	UFUNCTION(BlueprintCallable, Category="UI|Multiplayer") bool RequestJoin();

private:
	void Initialize(const FCC_RoomSummary& InRoom, UCC_MultiplayerScreenWidget* InOwner, bool bInCanJoin);
	UPROPERTY(Transient) FCC_RoomSummary Room;
	UPROPERTY(Transient) TObjectPtr<UCC_MultiplayerScreenWidget> OwnerScreen;
	bool bCanJoin = false;
	friend class UCC_MultiplayerScreenWidget;
};

/** ListView 行基类；每一行都拥有自己的 Button_JoinRoom。 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_MultiplayerRoomItemWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	/** 蓝图在这里把房间名称、人数和 Ping 写入行内文本控件。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Multiplayer")
	void OnRoomItemChanged(const FCC_RoomSummary& Room, bool bCanJoin);
	/** Designer 中必需的行内加入按钮。 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|Multiplayer")
	TObjectPtr<UCommonButtonBase> Button_JoinRoom;

private:
	UFUNCTION() void HandleJoinClicked();
	UPROPERTY(Transient) TObjectPtr<UCC_MultiplayerRoomEntry> RoomEntry;
};
