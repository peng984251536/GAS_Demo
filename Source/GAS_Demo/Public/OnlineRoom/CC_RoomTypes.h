#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "CC_RoomTypes.generated.h"

/**
 * 房间门面当前的异步状态。
 *
 * 显式状态机可以拒绝重复点击与互相冲突的请求，避免将 OSS 的异步回调
 * 直接暴露给 UI。
 */
UENUM(BlueprintType)
enum class ECC_RoomAsyncState : uint8
{
	Idle,
	Creating,
	Searching,
	Joining,
	Traveling,
	InRoom,
	Destroying,
	/** 残留会话需要 LeaveRoom 清理；禁止直接 Join/Host。 */
	RecoveryRequired
};

/** 用于 UI 和日志识别完成的具体操作。 */
UENUM(BlueprintType)
enum class ECC_RoomOperation : uint8
{
	None,
	Host,
	Search,
	Join,
	Leave
};

/** 稳定的业务错误码；UI 不需要理解不同在线后端的错误枚举。 */
UENUM(BlueprintType)
enum class ECC_RoomResultCode : uint8
{
	Success,
	Busy,
	InvalidRequest,
	BackendUnavailable,
	RoomNotFound,
	RoomFull,
	AlreadyInRoom,
	NetworkFailure,
	TravelFailure,
	UnknownFailure
};

/** 所有房间操作的统一结果，方便 CommonUI 使用同一套弹窗与错误处理。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_RoomOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	ECC_RoomResultCode Code = ECC_RoomResultCode::UnknownFailure;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FText Message;

	/** 命名工厂保证成功标志与错误码一致。 */
	static FCC_RoomOperationResult Success(const FText& InMessage = FText::GetEmpty())
	{
		FCC_RoomOperationResult Result;
		Result.bSuccess = true;
		Result.Code = ECC_RoomResultCode::Success;
		Result.Message = InMessage;
		return Result;
	}

	/** 命名工厂统一创建可传递到蓝图的失败结果。 */
	static FCC_RoomOperationResult Failure(ECC_RoomResultCode InCode, const FText& InMessage)
	{
		FCC_RoomOperationResult Result;
		Result.Code = InCode;
		Result.Message = InMessage;
		return Result;
	}
};

/** 创建房间所需的可配置快照。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_RoomCreateRequest
{
	GENERATED_BODY()

	/** 用于房间列表展示，不作为底层 Session 的唯一键。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	FString DisplayName = TEXT("Room");

	/** Session 创建成功后，房主以 listen server 方式打开此地图。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	TSoftObjectPtr<UWorld> LobbyMap;

	/** 只用于搜索列表展示和过滤，真正的规则由 Lobby GameMode 决定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	FName GameModeId = TEXT("Default");

	/** 客户端只加入协议版本相同的房间，建议发布时填入构建号。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	FString BuildId = TEXT("dev");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room", meta=(ClampMin="1", UIMin="1"))
	int32 MaxPlayers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	bool bIsLANMatch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	bool bAllowJoinInProgress = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	bool bShouldAdvertise = true;
};

/** 搜索策略。后端适配器会把它翻译成对应 OSS 查询。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_RoomSearchRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	FString BuildId = TEXT("dev");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room", meta=(ClampMin="1", ClampMax="500"))
	int32 MaxResults = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	bool bIsLANQuery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Online Room")
	bool bHideFullRooms = true;
};

/**
 * 面向 UI 的房间 DTO。
 *
 * 故意不暴露 FOnlineSessionSearchResult：这样 Widget 不会与某个在线实现耦合，
 * 也不会在下一次搜索后持有失效的底层对象。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_RoomSummary
{
	GENERATED_BODY()

	/** 一次搜索结果中的稳定键，JoinRoom 使用它回查底层结果。 */
	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FString RoomId;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FName MapId;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FName GameModeId;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	FString BuildId;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category="Online Room")
	bool bJoinable = false;
};

/** 大厅地图内的服务器权威阶段。 */
UENUM(BlueprintType)
enum class ECC_RoomLobbyPhase : uint8
{
	Waiting,
	Starting,
	Traveling
};
