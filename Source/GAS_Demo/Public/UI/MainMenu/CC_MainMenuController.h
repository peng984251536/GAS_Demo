#pragma once

#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "OnlineRoom/CC_RoomTypes.h"
#include "CC_MainMenuController.generated.h"

class UCC_ActivatableWidget;
class UCC_MainMenuModel;
class UCC_OnlineRoomSubsystem;

/** 主菜单展示快照；联网房间状态由 MultiplayerScreen 的模型独立维护。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_MainMenuState
{
	GENERATED_BODY()

	/** 房间服务是否存在；false 时搜索和离开入口不可用。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|MainMenu") bool bRoomServiceAvailable = false;
	/** 开始流程或房间事务尚未完成，用于禁止重复点击。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|MainMenu") bool bBusy = false;
	/** 主界面据此决定是否显示离开/清理入口。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|MainMenu") ECC_RoomAsyncState RoomState = ECC_RoomAsyncState::Idle;
	/** 最近一次搜索或离房结果，用于显示错误。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|MainMenu") FCC_RoomOperationResult LastRoomResult;
	/** 主菜单开始、设置或退出操作产生的可展示错误。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|MainMenu") FText ActionError;
};

/** 主菜单只读展示模型，不保存房间列表。 */
UCLASS()
class GAS_DEMO_API UCC_MainMenuModel : public UCC_UIModel
{
	GENERATED_BODY()

public:
	/** 返回当前完整快照，供原生界面或蓝图刷新。 */
	UFUNCTION(BlueprintPure, Category="UI|MainMenu")
	FCC_MainMenuState GetState() const { return State; }

private:
	/** 控制器一次性提交完整状态，避免 Widget 读到半更新数据。 */
	void SetState(const FCC_MainMenuState& Value) { State = Value; NotifyChanged(); }
	UPROPERTY(Transient) FCC_MainMenuState State;
	friend class UCC_MainMenuController;
};

/** 主菜单控制层：处理本地开始流程、设置页面和退出确认。 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_MainMenuController : public UCC_UIController
{
	GENERATED_BODY()

public:
	/** 设置主菜单模型与默认退出确认页面。 */
	UCC_MainMenuController();
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool StartLocalGame(TSoftObjectPtr<UWorld> Map);
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool HostRoom(const FCC_RoomCreateRequest& Request);
	/** 从主界面发起搜索，并把房间列表页面压入 Menu 层显示结果。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu")
	bool FindRooms(const FCC_RoomSearchRequest& Request, TSubclassOf<UCC_ActivatableWidget> RoomListScreenClass);
	/** 从主界面清理当前 Session，并返回指定前端地图。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu")
	bool LeaveRoom(TSoftObjectPtr<UWorld> FrontEndMap);
	/** 打开配置的设置页面；页面类为空时向模型报告错误。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool OpenSettings();
	/** 打开退出确认框，只有确认框才执行实际退出。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool RequestQuit();
	/** 请求开始/继续；统一防重入，具体存档系统由派生控制器接入。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool RequestStartGame(bool bContinue);
	/** 自定义开始流程完成时提交原始 RequestId；过期/重复回调被拒绝。 */
	UFUNCTION(BlueprintCallable, Category="UI|MainMenu") bool CompleteStartGame(FGuid RequestId, bool bSuccess, FText Error);

protected:
	/** 页面激活时监听房间服务，以更新搜索和离开按钮状态。 */
	virtual void OnActivated() override;
	/** 页面失活时解除房间监听并废弃页面级开始请求。 */
	virtual void OnDeactivated() override;
	/** 项目扩展点：调用存档/加载服务，结束时调用 CompleteStartGame。 */
	UFUNCTION(BlueprintNativeEvent, Category="UI|MainMenu")
	void ExecuteStartGame(FGuid RequestId, bool bContinue);
	/** 未配置业务时明确报告失败，不猜测地图或存档槽。 */
	virtual void ExecuteStartGame_Implementation(FGuid RequestId, bool bContinue);
	/** 设置页面类型，可在控制器蓝图默认值中配置。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|MainMenu") TSubclassOf<UCC_ActivatableWidget> SettingsScreenClass;
	/** Modal 层退出确认页面类型。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|MainMenu") TSubclassOf<UCC_ActivatableWidget> QuitScreenClass;

private:
	/** 房间状态改变时刷新主菜单按钮。 */
	UFUNCTION() void HandleRoomState(ECC_RoomAsyncState NewState);
	/** 搜索或离房结束时刷新结果消息。 */
	UFUNCTION() void HandleRoomResult(ECC_RoomOperation Operation, FCC_RoomOperationResult Result);
	/** 将开始流程和错误合并后提交给模型。 */
	void RefreshState();
	/** 只有当前主菜单位于栈顶且没有开始请求时允许操作。 */
	bool CanStartAction() const;
	/** 检查主菜单位于栈顶且房间服务可用。 */
	bool CanUseRoomService();
	/** 更新主菜单错误并触发展示刷新。 */
	void SetActionError(const FText& Error);
	/** 当前自定义开始请求 ID；完成或失活后作废。 */
	FGuid StartRequest;
	/** 记录请求所属激活会话，过滤页面重新打开前的迟到回调。 */
	FGuid StartActivation;
	/** 主菜单本地错误缓存。 */
	FText ActionError;
	/** 房间系统由 GameInstance 持有；主菜单仅在激活时监听。 */
	TWeakObjectPtr<UCC_OnlineRoomSubsystem> RoomService;
};
