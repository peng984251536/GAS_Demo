#pragma once

#include "CoreMinimal.h"
#include "OnlineRoom/CC_RoomTypes.h"

class UGameInstance;

/**
 * 房间会话后端的 Strategy 接口。
 *
 * UCC_OnlineRoomSubsystem 只依赖此抽象，经典 OnlineSubsystem、EOS 或自建后端可以
 * 各自提供 Adapter，而不改动 UI 和大厅规则。
 */
class GAS_DEMO_API ICC_RoomSessionBackend
{
public:
	DECLARE_DELEGATE_TwoParams(FOnOperationComplete, ECC_RoomOperation, const FCC_RoomOperationResult&);
	DECLARE_DELEGATE_TwoParams(FOnSearchComplete, const FCC_RoomOperationResult&, const TArray<FCC_RoomSummary>&);
	DECLARE_DELEGATE_TwoParams(FOnJoinComplete, const FCC_RoomOperationResult&, const FString&);

	/** 多态所有权由门面持有，确保派生适配器能完整释放。 */
	virtual ~ICC_RoomSessionBackend() = default;

	/** 注入跨地图的 GameInstance 上下文。 */
	virtual void Initialize(UGameInstance* InGameInstance) = 0;
	/** 解除所有底层异步委托，防止回调已销毁的门面。 */
	virtual void Shutdown() = 0;

	/** 建立发布会话；bool 表示请求是否受理，最终结果必须通过回调通知。 */
	virtual bool HostRoom(const FCC_RoomCreateRequest& Request) = 0;
	/** 搜索并缓存后端结果，把与平台无关的 DTO 返回给门面。 */
	virtual bool FindRooms(const FCC_RoomSearchRequest& Request) = 0;
	/** 消费本轮搜索令牌，成功回调提供连接地址；适配器不执行 Travel。 */
	virtual bool JoinRoom(const FString& RoomId) = 0;
	/** 退出/销毁当前 GameSession；不负责地图与 UI。 */
	virtual bool LeaveRoom() = 0;
	/** 判断失败后是否仍需要清理平台会话。 */
	virtual bool HasSession() const = 0;

	/** Host/Leave 完成回调；包含请求立即被拒绝的情况。 */
	virtual FOnOperationComplete& OnOperationComplete() = 0;
	/** Search 专用回调，不重复发送通用回调。 */
	virtual FOnSearchComplete& OnSearchComplete() = 0;
	/** Join 专用回调，返回地址或明确错误。 */
	virtual FOnJoinComplete& OnJoinComplete() = 0;
};
