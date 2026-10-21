#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "CC_RespawnComponent.generated.h"

class ACC_PlayerCharacter;
class ACC_SurvivalPlayerState;
class UCC_SurvivalModeConfig;

/**
 * 阵亡玩家的倒计时复活（挂在 ACC_SurvivalGameMode 上，仅服务器）。
 * 倒计时结束且死亡动作已播完 → 选一名随机存活队友 → 在其周围环上找导航可达、无阻挡的落点 →
 * ACC_PlayerCharacter::Revive。复用原 Pawn，不走 PossessedBy，因此升级 Buff 保留。
 * 不判断全灭；全灭由 GameMode 处理，GameOver 时调用 CancelAll。
 */
UCLASS(ClassGroup = (Survival))
class GAS_DEMO_API UCC_RespawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCC_RespawnComponent();

	void SetConfig(const UCC_SurvivalModeConfig* InConfig) { Config = InConfig; }

	/** 开始倒计时：按阵亡次数计算等待时间，写入 PlayerState.RespawnServerTime。 */
	void SchedulePlayer(ACC_SurvivalPlayerState* PlayerState);

	/** 死亡动作结束（LifeState 已由 GameMode 改为 Dead）；若倒计时已到则立即复活。 */
	void NotifyDeathFinished(ACC_SurvivalPlayerState* PlayerState);

	void Cancel(ACC_SurvivalPlayerState* PlayerState);
	void CancelAll();

	/** 立即复活（调试用）；不满足条件时返回 false。 */
	bool ReviveNow(ACC_SurvivalPlayerState* PlayerState);

	/**
	 * 在 Anchor 周围 [MinRadius, MaxRadius] 环上找复活落点（8 个方向，导航投影 + 胶囊无阻挡）。
	 * 全部失败时返回 Anchor 当前位置（与队友重叠，由移动组件推开）。
	 */
	FVector FindReviveLocation(const ACC_PlayerCharacter* Anchor, const ACC_PlayerCharacter* Reviving) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FPending
	{
		FTimerHandle CountdownTimer;
		FTimerHandle FallbackTimer;
		bool bCountdownDone = false;
	};

	void HandleCountdownFinished(TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState);
	void TryRevive(ACC_SurvivalPlayerState* PlayerState, bool bForce);
	ACC_PlayerCharacter* PickAnchor(const ACC_SurvivalPlayerState* Exclude);
	void ClearPending(ACC_SurvivalPlayerState* PlayerState);

	UPROPERTY(Transient)
	TObjectPtr<const UCC_SurvivalModeConfig> Config;

	TMap<TWeakObjectPtr<ACC_SurvivalPlayerState>, FPending> Pending;
	FRandomStream Random;
};
