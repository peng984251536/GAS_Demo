#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineRoom/CC_RoomSessionBackend.h"

class UGameInstance;

/**
 * ICC_RoomSessionBackend 的经典 OnlineSubsystem Adapter。
 *
 * 该类是纯 C++ 对象，专门隔离 OSS 类型、委托句柄与查询结果缓存。
 * 如果以后改用 Online Services/EOS 或自建后端，只需替换这个 Adapter。
 */
class FCC_OnlineSubsystemRoomBackend final : public ICC_RoomSessionBackend
{
public:
	using ThisClass = FCC_OnlineSubsystemRoomBackend;
	/** RAII 兜底解除底层委托；Shutdown 可重复调用。 */
	virtual ~FCC_OnlineSubsystemRoomBackend() override;

	/** 保存 World 上下文，避免全局 OSS 在 PIE 中串号。 */
	virtual void Initialize(UGameInstance* InGameInstance) override;
	/** 清理委托和搜索缓存，不发起退出时不可等待的异步销毁。 */
	virtual void Shutdown() override;
	/** 参数转 SessionSettings；残留同名 Session 先销毁再创建。 */
	virtual bool HostRoom(const FCC_RoomCreateRequest& Request) override;
	/** 创建有独立代次令牌的搜索；Null 结果还需要本地过滤。 */
	virtual bool FindRooms(const FCC_RoomSearchRequest& Request) override;
	/** 校验搜索令牌后加入 NAME_GameSession。 */
	virtual bool JoinRoom(const FString& RoomId) override;
	/** 将成员退出与房主销毁统一映射到 DestroySession。 */
	virtual bool LeaveRoom() override;
	/** 查询本地平台会话是否残留。 */
	virtual bool HasSession() const override;

	/** 返回 Host/Leave 完成观察接口。 */
	virtual FOnOperationComplete& OnOperationComplete() override { return OperationCompleteDelegate; }
	/** 返回 Search 完成观察接口。 */
	virtual FOnSearchComplete& OnSearchComplete() override { return SearchCompleteDelegate; }
	/** 返回 Join 完成观察接口。 */
	virtual FOnJoinComplete& OnJoinComplete() override { return JoinCompleteDelegate; }

private:
	enum class EDestroyPurpose : uint8
	{
		None,
		ReplaceBeforeCreate,
		Leave
	};

	/** 按当前 World 解析 OSS 实例；仅在新请求开始时调用。 */
	IOnlineSessionPtr ResolveSessionInterface();
	/** 配置发现字段并注册一次性创建回调。 */
	bool StartCreateSession(const FCC_RoomCreateRequest& Request);
	/** 销毁命令记录后续动作：离房，或清理后重新建房。 */
	bool StartDestroySession(EDestroyPurpose Purpose);
	/** 统一释放所有 Raw 回调句柄，满足适配器生命周期边界。 */
	void ClearOnlineDelegates();
	/** 对同步受理失败也提供一次完成通知。 */
	void FailToStart(ECC_RoomOperation Operation, ECC_RoomResultCode Code, const TCHAR* Message);

	/** 只消费本模块的命名 Session 完成结果。 */
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	/** 转换平台数据，过滤项目标记、版本及满员项。 */
	void HandleFindSessionsComplete(bool bWasSuccessful);
	/** 把平台错误映射为稳定业务错误，成功则解析连接地址。 */
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	/** 根据销毁目的继续建房或向门面报告离房结果。 */
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	TWeakObjectPtr<UGameInstance> GameInstance;
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> ActiveSearch;
	TMap<FString, int32> SearchResultIndexByRoomId;
	FCC_RoomCreateRequest PendingCreateRequest;
	FCC_RoomSearchRequest SearchRequest;
	FString SearchGeneration;
	EDestroyPurpose DestroyPurpose = EDestroyPurpose::None;

	FDelegateHandle CreateDelegateHandle;
	FDelegateHandle FindDelegateHandle;
	FDelegateHandle JoinDelegateHandle;
	FDelegateHandle DestroyDelegateHandle;

	FOnOperationComplete OperationCompleteDelegate;
	FOnSearchComplete SearchCompleteDelegate;
	FOnJoinComplete JoinCompleteDelegate;
};
