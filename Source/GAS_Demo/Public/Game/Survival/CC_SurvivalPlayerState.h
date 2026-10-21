#pragma once

#include "CoreMinimal.h"
#include "Game/CC_RoomFramework.h"
#include "Game/Survival/CC_StatTagStack.h"
#include "Game/Survival/CC_SurvivalTypes.h"
#include "CC_SurvivalPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_SurvivalPlayerStateChanged);

/**
 * 生存模式的玩家状态：个人统计、生命状态、复活时间。
 * 继承房间 PlayerState（房主标志、准备状态、无缝切图复制）；本局统计不跨局复制。
 * 结算页的"再来一局 / 返回大厅"请求也经由这里的 Server RPC 发给服务器（PlayerState 归本连接所有）。
 */
UCLASS()
class GAS_DEMO_API ACC_SurvivalPlayerState : public ACC_RoomPlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** 无缝切图到新一局：只沿用房间数据，不复制统计与生命状态。 */
	virtual void CopyProperties(APlayerState* NewState) override;

	// --- 读取（客户端 / 服务器） ---

	UFUNCTION(BlueprintPure, Category = "Survival")
	int32 GetStat(FGameplayTag Tag) const { return Stats.GetStackCount(Tag); }

	UFUNCTION(BlueprintPure, Category = "Survival")
	ECC_PlayerLifeState GetLifeState() const { return LifeState; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsAliveInMatch() const { return LifeState == ECC_PlayerLifeState::Alive; }

	/** 复活的服务器时间点；0 表示没有在倒计时。 */
	UFUNCTION(BlueprintPure, Category = "Survival")
	double GetRespawnServerTime() const { return RespawnServerTime; }

	/** 距离复活还有几秒（按服务器时钟）；没有在倒计时返回 0。 */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetRespawnRemainingSeconds() const;

	const FCC_StatTagStackContainer& GetStats() const { return Stats; }

	/** 统计、生命状态或复活时间变化（服务器本地修改与客户端复制都会广播）。 */
	UPROPERTY(BlueprintAssignable, Category = "Survival")
	FCC_SurvivalPlayerStateChanged OnSurvivalStateChanged;

	// --- 修改（仅服务器） ---

	void AddStat(FGameplayTag Tag, int32 Count);
	void SetLifeState(ECC_PlayerLifeState NewState);
	void SetRespawnServerTime(double ServerTime);
	/** 新一局开始：清空统计、置为存活。 */
	void ResetForMatch();

	// --- 结算页请求（客户端调用自己的 PlayerState） ---

	/** 房主请求以同一地图再来一局；非房主的请求被忽略。 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Survival")
	void ServerRequestPlayAgain();

	/** 房主请求全员返回大厅；非房主的请求被忽略。 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Survival")
	void ServerRequestReturnToLobby();

	/** 本玩家是否有权发起"再来一局 / 返回大厅"（房主，或单机/监听服务器的本地玩家）。 */
	UFUNCTION(BlueprintPure, Category = "Survival")
	bool CanControlMatch() const;

private:
	UFUNCTION()
	void OnRep_SurvivalState();
	void NotifySurvivalStateChanged();

	UPROPERTY(ReplicatedUsing = OnRep_SurvivalState)
	FCC_StatTagStackContainer Stats;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivalState)
	ECC_PlayerLifeState LifeState = ECC_PlayerLifeState::Alive;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivalState)
	double RespawnServerTime = 0.0;
};
