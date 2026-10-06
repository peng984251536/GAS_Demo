#pragma once

#include "UI/Framework/CC_UIController.h"
#include "UI/Framework/CC_UIModel.h"
#include "OnlineRoom/CC_RoomTypes.h"
#include "CC_MultiplayerScreenController.generated.h"

class UCC_OnlineRoomSubsystem;
class UCC_MultiplayerScreenModel;

/** 房间列表页的一次完整展示快照；真实 Session 数据仍由 OnlineRoomSubsystem 持有。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_MultiplayerScreenState
{
	GENERATED_BODY()

	/** 房间服务是否可用；false 时页面应显示配置错误。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Multiplayer") bool bRoomServiceAvailable = false;
	/** 搜索或加入正在执行，用于禁用重复操作。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Multiplayer") bool bBusy = false;
	/** 房间服务当前阶段；只有 Idle 状态允许刷新或加入。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Multiplayer") ECC_RoomAsyncState RoomState = ECC_RoomAsyncState::Idle;
	/** 当前一轮搜索结果的展示副本；再次搜索后旧 RoomId 失效。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Multiplayer") TArray<FCC_RoomSummary> Rooms;
	/** 最近一次房间操作结果，供页面显示消息和稳定错误码。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Multiplayer") FCC_RoomOperationResult LastRoomResult;
	/** 页面控制层产生的本地错误，例如房间服务不存在。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Multiplayer") FText ActionError;
};

/** 房间列表展示模型：只保存 UI 快照，不拥有 Session 或底层搜索对象。 */
UCLASS()
class GAS_DEMO_API UCC_MultiplayerScreenModel : public UCC_UIModel
{
	GENERATED_BODY()

public:
	/** 返回当前完整状态，蓝图用它刷新列表、进度和错误提示。 */
	UFUNCTION(BlueprintPure, Category="UI|Multiplayer")
	FCC_MultiplayerScreenState GetState() const { return State; }

private:
	/** 仅控制器可以替换快照；Widget 没有业务写权限。 */
	void SetState(const FCC_MultiplayerScreenState& Value) { State = Value; NotifyChanged(); }
	UPROPERTY(Transient) FCC_MultiplayerScreenState State;
	friend class UCC_MultiplayerScreenController;
};

/** 房间列表页控制层：监听房间服务，并把用户操作转发给统一房间门面。 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_MultiplayerScreenController : public UCC_UIController
{
	GENERATED_BODY()

public:
	/** 指定多人页面模型，页面实例之间互不共享展示状态。 */
	UCC_MultiplayerScreenController();
	UFUNCTION(BlueprintCallable, Category="UI|Multiplayer") bool RefreshRooms(const FCC_RoomSearchRequest& Request);
	/** 使用本轮搜索产生的 RoomId 加入房间。 */
	UFUNCTION(BlueprintCallable, Category="UI|Multiplayer") bool JoinRoom(const FString& RoomId);

protected:
	/** 激活时订阅房间事件，并主动读取一次已有快照。 */
	virtual void OnActivated() override;
	/** 页面失活时解除所有房间事件，不取消正在跨地图执行的事务。 */
	virtual void OnDeactivated() override;

private:
	/** 状态变化时重新读取完整服务快照，避免 UI 拼接半状态。 */
	UFUNCTION() void HandleRoomState(ECC_RoomAsyncState NewState);
	/** 操作完成时刷新结果、搜索列表和可交互状态。 */
	UFUNCTION() void HandleRoomResult(ECC_RoomOperation Operation, FCC_RoomOperationResult Result);
	/** 将当前服务状态和本地错误作为一个完整快照提交给模型。 */
	void RefreshState();
	/** 校验页面位于栈顶、服务存在且没有其他房间事务。 */
	bool CanUseRoomService();
	/** 记录本地错误并立即刷新页面。 */
	void SetActionError(const FText& Error);
	/** GameInstance 拥有房间服务；页面控制器只在激活期间弱引用。 */
	TWeakObjectPtr<UCC_OnlineRoomSubsystem> RoomService;
	/** 页面自身产生的错误缓存。 */
	FText ActionError;
};
