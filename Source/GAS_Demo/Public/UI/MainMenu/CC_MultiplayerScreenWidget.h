#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/MainMenu/CC_MultiplayerScreenController.h"
#include "CC_MultiplayerScreenWidget.generated.h"

class UCommonButtonBase;
class UListView;
class UCC_MultiplayerRoomEntry;

/** LAN 房间浏览页：搜索房间，并通过滚动列表条目的加入按钮进入房间。 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_MultiplayerScreenWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()

public:
	UCC_MultiplayerScreenWidget(const FObjectInitializer& ObjectInitializer);
	/** 重新搜索同一局域网内、BuildId 匹配的房间。 */
	UFUNCTION(BlueprintCallable, Category="UI|Multiplayer") bool RefreshRooms();
	/** 供房间列表条目调用；RoomId 必须仍属于当前搜索结果。 */
	UFUNCTION(BlueprintCallable, Category="UI|Multiplayer") bool JoinRoom(const FString& RoomId);
	/** 关闭房间浏览页，返回主菜单。 */
	UFUNCTION(BlueprintCallable, Category="UI|Multiplayer") bool GoBack();
	UFUNCTION(BlueprintPure, Category="UI|Multiplayer") FCC_MultiplayerScreenState GetMultiplayerState() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnModelChanged() override;

	/** LAN 搜索参数；调用时始终强制 bIsLANQuery=true。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|Multiplayer")
	FCC_RoomSearchRequest SearchRequest;
	/** 必需的滚动列表；Entry Widget Class 应继承 CC_MultiplayerRoomItemWidget。 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|Multiplayer")
	TObjectPtr<UListView> List_Rooms;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|Multiplayer")
	TObjectPtr<UCommonButtonBase> Button_Refresh;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|Multiplayer")
	TObjectPtr<UCommonButtonBase> Button_Back;

	/** 用于刷新加载动画、空列表提示和错误文字；房间行由 List_Rooms 自动创建。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Multiplayer")
	void OnMultiplayerStateChanged(const FCC_MultiplayerScreenState& State);

private:
	UCC_MultiplayerScreenController* GetMultiplayerController() const;
	void RefreshScreen();
	UFUNCTION() void HandleRefreshClicked();
	UFUNCTION() void HandleBackClicked();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UCC_MultiplayerRoomEntry>> RoomEntries;
};
