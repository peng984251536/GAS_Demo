#pragma once

#include "CoreMinimal.h"
#include "ActiveGameplayEffectHandle.h"
#include "Engine/TimerHandle.h"
#include "Game/CC_RoomFramework.h"
#include "CC_SurvivalGameMode.generated.h"

class ACC_BaseCharacter;
class ACC_SurvivalGameState;
class ACC_SurvivalPlayerState;
class UAbilitySystemComponent;
class UCC_BuffSelectionComponent;
class UCC_RespawnComponent;
class UCC_SurvivalModeConfig;
struct FCC_BuffOfferOption;
struct FCC_DeathInfo;
struct FGameplayEventData;

/**
 * 生存模式规则（仅服务器）：对局阶段、死亡事件路由、击杀归属、全灭判定、统计记录、复活。
 * 继承 ACC_RoomGameMode，复用房间准入（中途加入由房主选项决定）、无缝切图与返回大厅。
 * 波次与经验逻辑在 ACC_SurvivalGameState 的两个组件里，本类只负责把事件转给它们。
 * 角色不知道自己处于生存模式：死亡通过 UCC_GameEventSubsystem 转发到这里。
 * 设计见 Docs/Gameplay/SurvivalMode.zh-CN.md。
 */
UCLASS()
class GAS_DEMO_API ACC_SurvivalGameMode : public ACC_RoomGameMode
{
	GENERATED_BODY()

public:
	ACC_SurvivalGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** 开始对局（通常自动：所有玩家就绪或等待超时后）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void StartMatch();

	/** 结束对局并结算（全灭时自动调用）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Survival")
	void EndMatch();

	UFUNCTION(BlueprintPure, Category = "Survival")
	bool IsMatchPlaying() const;

	const UCC_SurvivalModeConfig* GetConfig() const { return Config; }
	UCC_RespawnComponent* GetRespawn() const { return Respawn; }
	ACC_SurvivalGameState* GetSurvivalGameState() const;

	/** 复活组件完成复活后调用：恢复存活状态、统计、提示、视角。 */
	void HandlePlayerRevived(ACC_SurvivalPlayerState* PlayerState);

	/** 结算页请求（已在 PlayerState 校验房主身份）。 */
	void RequestPlayAgain();
	void RequestReturnToLobby();

	// --- 调试（控制台命令 cc.Survival.*，仅服务器） ---
	void DebugAddExperience(float Amount);
	void DebugReviveAll();
	void DebugToggleGodMode();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 默认配置；地图 URL 选项 ?SurvivalConfig= 可覆盖。 */
	UPROPERTY(EditDefaultsOnly, Category = "Survival")
	TObjectPtr<UCC_SurvivalModeConfig> Config;

	/** 等待所有玩家 Pawn 就绪的最长秒数，超时后直接开始。 */
	UPROPERTY(EditDefaultsOnly, Category = "Survival", meta = (ClampMin = "0", Units = "s"))
	float StartTimeout = 10.f;

	UPROPERTY(VisibleAnywhere, Category = "Survival")
	TObjectPtr<UCC_RespawnComponent> Respawn;

private:
	/** 每个玩家的事件订阅与选卡保护句柄；加入时创建，离开/结算时对称回收。 */
	struct FPlayerBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FDelegateHandle DamageDealtHandle;
		FDelegateHandle DamageTakenHandle;
		TWeakObjectPtr<UCC_BuffSelectionComponent> Selection;
		FDelegateHandle PendingHandle;
		FDelegateHandle AcceptedHandle;
		FActiveGameplayEffectHandle ProtectionHandle;
	};

	void TryStartMatch();
	bool AreAllPlayersReady() const;

	void HandleCharacterDied(const FCC_DeathInfo& Info);
	void HandleCharacterDeathFinished(ACC_BaseCharacter* Character);
	void HandleEnemyDied(const FCC_DeathInfo& Info);
	void HandlePlayerDied(ACC_SurvivalPlayerState* PlayerState);
	bool ShouldCreditKill(const FCC_DeathInfo& Info) const;
	bool IsTeamWiped() const;
	void CheckTeamWipedNextTick();
	void SpectateTeammate(ACC_SurvivalPlayerState* PlayerState);
	void PlaceNearTeammate(APlayerController* Player);

	void BindPlayer(APlayerController* Player);
	void UnbindPlayer(ACC_SurvivalPlayerState* PlayerState);
	void UnbindAllPlayers();
	void HandleDamageEvent(const FGameplayEventData* Payload, TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState, bool bDealt);
	void HandleOfferPendingChanged(bool bHasPending, TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState);
	void HandleChoiceAccepted(const FCC_BuffOfferOption& Option, TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState);
	void SetChoosingProtection(ACC_SurvivalPlayerState* PlayerState, bool bEnable);
	void FreezeEnemies();
	void UpdateDebugDisplay();

	TMap<TWeakObjectPtr<ACC_SurvivalPlayerState>, FPlayerBinding> Bindings;
	FDelegateHandle DiedHandle;
	FDelegateHandle DeathFinishedHandle;
	FTimerHandle StartCheckTimer;
	FTimerHandle DebugDisplayTimer;
	double WaitStartTime = 0.0;
	int32 Seed = 0;
	bool bSeedFromOptions = false;
};
