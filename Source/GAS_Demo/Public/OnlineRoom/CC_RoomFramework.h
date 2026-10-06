#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "Player/CC_PlayerState.h"
#include "Player/CC_PlayerController.h"
#include "OnlineRoom/CC_RoomTypes.h"
#include "CC_RoomFramework.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_LobbyChanged);

/** 大厅成员状态。继承项目 GAS PlayerState，保持现有 ASC 所有权与角色接口。 */
UCLASS()
class GAS_DEMO_API ACC_RoomPlayerState : public ACC_PlayerState
{
	GENERATED_BODY()
public:
	/** 用属性复制传播持久状态；迟加入的玩家也能取得完整快照。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** PlayerState 被无缝切图替换时复制业务数据，准备标志不带入新一局。 */
	virtual void CopyProperties(APlayerState* NewState) override;
	/** UI 查询当前准备状态。 */
	UFUNCTION(BlueprintPure, Category="Online Room|Lobby") bool IsReady() const { return bReady; }
	/** UI 查询服务器分配的房主标志。 */
	UFUNCTION(BlueprintPure, Category="Online Room|Lobby") bool IsRoomHost() const { return bHost; }
	/** 仅供服务器规则层修改；客户端通过自己的 Controller 发请求。 */
	void SetReady(bool bValue);
	/** 仅供服务器设置房主身份，不提供客户端写入口。 */
	void SetHost(bool bValue);
	UPROPERTY(BlueprintAssignable, Category="Online Room|Lobby") FCC_LobbyChanged OnMemberChanged;
private:
	/** RepNotify 和监听服务器本地修改共用通知路径。 */
	UFUNCTION() void NotifyChanged();
	UPROPERTY(ReplicatedUsing=NotifyChanged) bool bReady = false;
	UPROPERTY(ReplicatedUsing=NotifyChanged) bool bHost = false;
};

/** 全员可读的大堂模型；Widget 订阅变化后从 PlayerArray 重新读取成员。 */
UCLASS()
class GAS_DEMO_API ACC_RoomLobbyState : public AGameStateBase
{
	GENERATED_BODY()
public:
	/** 注册共享房间阶段和容量的复制。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 成员到达时通知 UI；每个 PlayerState 的属性可能稍后才复制到达。 */
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	/** 成员离开时通知 UI。 */
	virtual void RemovePlayerState(APlayerState* PlayerState) override;
	/** 供 UI 获取强类型成员列表，避免依赖控制器遍历。 */
	UFUNCTION(BlueprintPure, Category="Online Room|Lobby") TArray<ACC_RoomPlayerState*> GetMembers() const;
	/** 当前大厅是否仍可变更准备状态。 */
	UFUNCTION(BlueprintPure, Category="Online Room|Lobby") ECC_RoomLobbyPhase GetPhase() const { return Phase; }
	/** 服务器初始化房间容量。 */
	void SetCapacity(int32 Value);
	/** 服务器唯一阶段写入口。 */
	void SetPhase(ECC_RoomLobbyPhase Value);
	/** 本地及复制修改统一广播，UI 无需轮询 Tick。 */
	UFUNCTION() void NotifyChanged();
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=NotifyChanged, Category="Online Room|Lobby") int32 Capacity = 4;
	UPROPERTY(BlueprintAssignable, Category="Online Room|Lobby") FCC_LobbyChanged OnLobbyChanged;
private:
	UPROPERTY(ReplicatedUsing=NotifyChanged) ECC_RoomLobbyPhase Phase = ECC_RoomLobbyPhase::Waiting;
};

/**
 * 客户端命令入口。Controller 归当前连接所有，因此 Server RPC 可验证发起者身份。
 * 继承项目控制器以复用战斗输入，服务器业务由 GameMode 执行。
 */
UCLASS()
class GAS_DEMO_API ACC_RoomPlayerController : public ACC_PlayerController
{
	GENERATED_BODY()
public:
	/** UI 提交自己的准备意图；服务器校验是否处于大厅等待阶段。 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Online Room|Lobby") void ServerSetReady(bool bReady);
	/** 请求开始；即使客户端伪造调用也必须通过房主/人数/准备校验。 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category="Online Room|Lobby") void ServerStartRoomGame();
	/** 每次到达 Lobby/Game 后同步房间连接状态，不参与 UI 创建或重建。 */
	UFUNCTION(Client, Reliable) void ClientApplyRoomPhase(bool bLobby);
	/** 请求被拒绝的本地通知；蓝图可显示原因。 */
	UFUNCTION(Client, Reliable) void ClientRoomCommandFailed(const FText& Reason);
	/** 页面只处理显示，不参与服务器权限判断。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Online Room|Lobby") void OnRoomCommandFailed(const FText& Reason);
};

/**
 * 联机地图共用规则基类（Template Method）。大厅与战斗共享连接准入、UI 和返回逻辑。
 * 新建战斗 GameMode 蓝图继承本类，并填入项目原来的 Pawn/Controller/PlayerState 配置。
 */
UCLASS()
class GAS_DEMO_API ACC_RoomGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	/** 默认启用无缝切图并使用房间专用控制器和 GAS PlayerState。 */
	ACC_RoomGameMode();
	/** 服务器拒绝满员和不允许的战斗中途加入；客户端显示只作提示。 */
	virtual void PreLogin(const FString& Options, const FString& Address,
		const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	/** 同时覆盖首次登录和无缝切图到达；通过模板方法决定是否生成战斗 Pawn。 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	/** 由服务器结算逻辑调用，所有连接一起返回指定大厅；不销毁 Session。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Online Room") bool ReturnToLobby();
protected:
	/** 子类只定义当前是否为大厅，连接通用逻辑保持在基类。 */
	virtual bool IsLobbyMap() const { return false; }
	/** 准入扩展点：子类可附加阶段等业务限制。 */
	virtual bool CanAcceptPlayer() const;
};

/** 大厅规则聚合根：准备与开始的权限判断只在服务器执行。 */
UCLASS()
class GAS_DEMO_API ACC_RoomLobbyMode : public ACC_RoomGameMode
{
	GENERATED_BODY()
public:
	/** 大厅不生成战斗角色，避免准备期间授予技能和初始化战斗属性。 */
	ACC_RoomLobbyMode();
	/** 把建房容量同步到 LobbyState。 */
	virtual void InitGameState() override;
	/** 首次登录/回大厅时重新确认房主并清除准备状态。 */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	/** 处理该连接的准备意图；仅修改其自身 PlayerState。 */
	bool SetMemberReady(APlayerController* Requester, bool bReady);
	/** 执行开始事务：权限校验 -> 锁定大厅 -> ServerTravel，失败恢复等待阶段。 */
	bool StartRoomGame(APlayerController* Requester, FText& OutReason);
protected:
	virtual bool IsLobbyMap() const override { return true; }
	virtual bool CanAcceptPlayer() const override;
	/** 策略扩展点：可增加队伍平衡等检查；原生基础校验仍会执行。 */
	UFUNCTION(BlueprintNativeEvent, Category="Online Room|Rules") bool ValidateAdditionalStartRules(FText& OutReason) const;
	/** 服务器配置的目标地图；不接受客户端传任意 URL。 */
	UPROPERTY(EditDefaultsOnly, Category="Online Room|Rules") TSoftObjectPtr<UWorld> GameMap;
	UPROPERTY(EditDefaultsOnly, Category="Online Room|Rules", meta=(ClampMin="1")) int32 MinimumPlayers = 1;
};
