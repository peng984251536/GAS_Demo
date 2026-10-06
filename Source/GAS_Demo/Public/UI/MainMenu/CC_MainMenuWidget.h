#pragma once

#include "UI/Framework/CC_ActivatableWidget.h"
#include "OnlineRoom/CC_RoomTypes.h"
#include "UI/MainMenu/CC_MainMenuController.h"
#include "CC_MainMenuWidget.generated.h"

class UCommonButtonBase;

/** 主菜单展示基类：只负责主菜单，不承载房间搜索和房间列表。 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_MainMenuWidget : public UCC_ActivatableWidget
{
	GENERATED_BODY()

public:
	/** 设置主菜单控制器，并将主菜单配置为不可用返回键关闭的底页。 */
	UCC_MainMenuWidget(const FObjectInitializer& ObjectInitializer);
	/** 主界面搜索按钮入口：发起搜索并打开配置的房间列表页面。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool SearchRooms();
	/** 主界面离开按钮入口：清理当前房间并返回前端地图。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool LeaveCurrentRoom();
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool StartSinglePlayer();
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool HostLANRoom();
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool OpenSettings();
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool RequestQuit();
	UFUNCTION(BlueprintPure, Category="UI|MainMenu") FCC_MainMenuState GetMainMenuState() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnModelChanged() override;
	UFUNCTION(BlueprintImplementableEvent, Category="UI|MainMenu")
	void OnMainMenuStateChanged(const FCC_MainMenuState& State);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|MainMenu") TSoftObjectPtr<UWorld> SinglePlayerMap;
	/** LobbyMap must use a Room GameMode so the server can confirm arrival. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|MainMenu") FCC_RoomCreateRequest HostRequest;
	/** Designer 中必须存在的 CommonUI 按钮；可使用任意 UCommonButtonBase 蓝图子类。 */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|MainMenu") TObjectPtr<UCommonButtonBase> Button_SinglePlayer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|MainMenu") TObjectPtr<UCommonButtonBase> Button_HostRoom;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|MainMenu") TObjectPtr<UCommonButtonBase> Button_Multiplayer;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|MainMenu") TObjectPtr<UCommonButtonBase> Button_LeaveRoom;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget), Category="UI|MainMenu") TObjectPtr<UCommonButtonBase> Button_Quit;
	/** LAN 搜索默认参数；BuildId 要与房主一致。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="UI|MainMenu") FCC_RoomSearchRequest SearchRequest;
	/** 搜索后显示的独立房间列表页面。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|MainMenu")
	TSubclassOf<UCC_ActivatableWidget> RoomListScreenClass;
	/** 离开房间后返回的前端地图。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|MainMenu") TSoftObjectPtr<UWorld> FrontEndMap;

private:
	void RefreshScreen();
	UFUNCTION() void HandleSinglePlayerClicked();
	UFUNCTION() void HandleHostClicked();
	UFUNCTION() void HandleMultiplayerClicked();
	UFUNCTION() void HandleLeaveClicked();
	UFUNCTION() void HandleQuitClicked();
	/** 获取页面框架创建的主菜单控制器。 */
	class UCC_MainMenuController* GetMainMenuController() const;
};
