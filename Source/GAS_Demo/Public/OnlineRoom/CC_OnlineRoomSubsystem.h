#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "OnlineRoom/CC_RoomTypes.h"
#include "OnlineRoom/CC_RoomSessionBackend.h"
#include "CC_OnlineRoomSubsystem.generated.h"

class ICC_RoomSessionBackend;
class UNetDriver;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCC_RoomOperationCompletedEvent, ECC_RoomOperation, Operation,
	FCC_RoomOperationResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCC_RoomSearchCompletedEvent, FCC_RoomOperationResult, Result,
	const TArray<FCC_RoomSummary>&, Rooms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCC_RoomStateChangedEvent, ECC_RoomAsyncState, NewState);

/**
 * 房间功能的 Facade（门面）。
 *
 * UI 只调用本类并监听统一事件。它负责驱动状态机、组合 Session 和 Travel，
 * 具体在线 API 由 ICC_RoomSessionBackend 适配器完成。作为 GameInstanceSubsystem，
 * 它在 FrontEnd -> Lobby -> Game 切图时不会被销毁。
 */
UCLASS()
class GAS_DEMO_API UCC_OnlineRoomSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 由服务器确认的 Controller 到达通知；JoinSession 成功并不等于连接已登录。 */
	void NotifyRoomEntered();
	/** 服务器的大堂规则读取建房参数；客户端不能用此本地快照作权威。 */
	const FCC_RoomCreateRequest& GetHostRequest() const { return PendingHostRequest; }
	/** 本地错误缓存，在失败引起切图、UI 重建后仍可读取。 */
	UFUNCTION(BlueprintPure, Category="Online Room")
	FCC_RoomOperationResult GetLastResult() const { return LastResult; }
	/** 建立适配器并监听引擎失败/切图事件。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** 按注册的句柄精确解绑，避免 PIE 停止后回调悬空对象。 */
	virtual void Deinitialize() override;

	/** 创建 Session，成功后以 listen server 方式进入 Request.LobbyMap。 */
	UFUNCTION(BlueprintCallable, Category="Online Room")
	bool HostRoom(const FCC_RoomCreateRequest& Request);

	/** 异步搜索房间；结果由 OnSearchCompleted 一次性返回。 */
	UFUNCTION(BlueprintCallable, Category="Online Room")
	bool FindRooms(const FCC_RoomSearchRequest& Request);

	/** 使用 FCC_RoomSummary.RoomId 加入最近一次搜索的房间。 */
	UFUNCTION(BlueprintCallable, Category="Online Room")
	bool JoinRoom(const FString& RoomId);

	/** 销毁/退出本地 Session，然后返回指定的本地前端地图。 */
	UFUNCTION(BlueprintCallable, Category="Online Room")
	bool LeaveRoom(TSoftObjectPtr<UWorld> FrontEndMap);

	/** UI 激活时先读快照，再订阅事件，避免漏掉先前的状态变化。 */
	UFUNCTION(BlueprintPure, Category="Online Room")
	ECC_RoomAsyncState GetState() const { return State; }

	/** 是否正在执行异步操作；RecoveryRequired 需要显示清理入口。 */
	UFUNCTION(BlueprintPure, Category="Online Room")
	bool IsBusy() const;

	/** 返回最近一次搜索结果；RoomId 只对该轮搜索有效。 */
	UFUNCTION(BlueprintPure, Category="Online Room")
	const TArray<FCC_RoomSummary>& GetCachedRooms() const { return CachedRooms; }

	/** 任意 Host/Search/Join/Leave 流程的最终结果。 */
	UPROPERTY(BlueprintAssignable, Category="Online Room")
	FCC_RoomOperationCompletedEvent OnOperationCompleted;

	/** 搜索完成的专用事件，适合直接刷新房间列表。 */
	UPROPERTY(BlueprintAssignable, Category="Online Room")
	FCC_RoomSearchCompletedEvent OnSearchCompleted;

	/** UI 可据此显示加载遮罩并禁用重复操作。 */
	UPROPERTY(BlueprintAssignable, Category="Online Room")
	FCC_RoomStateChangedEvent OnStateChanged;

protected:
	/** Factory Method：C++ 子类可注入其他后端或测试替身。 */
	virtual TUniquePtr<ICC_RoomSessionBackend> CreateBackend();
	/** 有派生门面时只实例化派生类，避免两套对象竞争 GameSession。 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	/** 防止同步事件监听者在一次事务尚未结束时重入。 */
	bool bDispatching = false;
	/** 使用实时时钟检查 Travel，不受前端菜单暂停影响。 */
	bool TickTravelTimeout(float DeltaSeconds);
	double TravelDeadline = 0.0;
	FTSTicker::FDelegateHandle TravelTickerHandle;
	FCC_RoomOperationResult LastResult;
	/** 只报告拒绝，不改变正在执行的事务。 */
	bool Reject(ECC_RoomOperation Operation, ECC_RoomResultCode Code, const TCHAR* Message);
	enum class EPendingTravel : uint8
	{
		None,
		EnterRoom,
		ReturnToFrontEnd
	};

	/** 集中执行状态转移校验，拒绝重入不会覆盖正在进行的事务。 */
	bool BeginOperation(ECC_RoomOperation Operation, ECC_RoomAsyncState NewState);
	/** 唯一状态写入口，广播给 Observer。 */
	void SetState(ECC_RoomAsyncState NewState);
	/** 结束活动事务，根据残留 Session 决定 Idle 或 RecoveryRequired。 */
	void FinishOperation(ECC_RoomOperation Operation, const FCC_RoomOperationResult& Result);
	/** 编排 Host/Leave 的平台完成与本地地图切换。 */
	void HandleBackendOperation(ECC_RoomOperation Operation, const FCC_RoomOperationResult& Result);
	/** 缓存 DTO，并向 UI 发布一次搜索完成。 */
	void HandleSearchComplete(const FCC_RoomOperationResult& Result, const TArray<FCC_RoomSummary>& Rooms);
	/** Join 成功后连接服务器，仍等待服务器 Controller 确认才宣布进房成功。 */
	void HandleJoinComplete(const FCC_RoomOperationResult& Result, const FString& ConnectString);
	/** 只确认返回本地前端地图；加载地图不代表联网登录成功。 */
	void HandlePostLoadMap(UWorld* LoadedWorld);
	/** 按 GameInstance 隔离引擎全局网络错误，防止多 PIE 串扰。 */
	void HandleNetworkFailure(UWorld* FailedWorld, UNetDriver* NetDriver, ENetworkFailure::Type FailureType,
		const FString& ErrorString);
	/** 把引擎切图失败转换为可重建 UI 后读取的业务错误。 */
	void HandleTravelFailure(UWorld* FailedWorld, ETravelFailure::Type FailureType, const FString& ErrorString);
	/** 验证软地图路径并发起本地 Travel；不会同步加载地图资产。 */
	bool TravelToLocalMap(const TSoftObjectPtr<UWorld>& Map, const FString& Options, EPendingTravel TravelPurpose);

	TUniquePtr<ICC_RoomSessionBackend> Backend;
	TArray<FCC_RoomSummary> CachedRooms;
	UPROPERTY(Transient) FCC_RoomCreateRequest PendingHostRequest;
	UPROPERTY(Transient) TSoftObjectPtr<UWorld> PendingFrontEndMap;
	ECC_RoomAsyncState State = ECC_RoomAsyncState::Idle;
	ECC_RoomOperation ActiveOperation = ECC_RoomOperation::None;
	EPendingTravel PendingTravel = EPendingTravel::None;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	friend class FCCRoomStateTest;
};
