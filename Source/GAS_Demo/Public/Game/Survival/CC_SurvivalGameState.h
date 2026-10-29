#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/CC_GameState.h"
#include "Game/Survival/CC_StatTagStack.h"
#include "Game/Survival/CC_SurvivalTypes.h"
#include "CC_SurvivalGameState.generated.h"

class ACC_SurvivalPlayerState;
class UCC_ActivatableWidget;
class UCC_TeamProgressionComponent;
class UCC_WaveDirectorComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCC_MatchPhaseChanged, ECC_MatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_TeamStatsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCC_MatchMessageReceived, const FCC_MatchMessage&, Message);

/**
 * 生存模式的全局复制状态：阶段、开始/结束时间、全队统计，以及挂在上面的
 * 波次组件（UCC_WaveDirectorComponent）与经验组件（UCC_TeamProgressionComponent）。
 * 服务器逻辑在 GameMode 和两个组件里；这里只保存所有客户端都需要看到的数据。
 * 进入 GameOver 时为本地玩家打开结算页（ResultScreenClass）。
 */
UCLASS()
class GAS_DEMO_API ACC_SurvivalGameState : public ACC_GameState
{
	GENERATED_BODY()

public:
	ACC_SurvivalGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Survival")
	UCC_WaveDirectorComponent* GetWaveDirector() const { return WaveDirector; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	UCC_TeamProgressionComponent* GetProgression() const { return Progression; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	ECC_MatchPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Survival")
	int32 GetTeamStat(FGameplayTag Tag) const { return TeamStats.GetStackCount(Tag); }

	const FCC_StatTagStackContainer& GetTeamStats() const { return TeamStats; }

	/** 已进行的秒数（GameOver 后固定为整局时长），按服务器时钟。 */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetMatchElapsedSeconds() const;

	/** 距离某个服务器时间点还有几秒；已过去返回 0。用于 HUD 倒计时。 */
	UFUNCTION(BlueprintPure, Category = "Survival")
	float GetSecondsUntilServerTime(double ServerTime) const;

	/** 收集所有生存模式玩家（不含观战者）。 */
	TArray<ACC_SurvivalPlayerState*> GetSurvivalPlayers() const;

	UPROPERTY(BlueprintAssignable, Category = "Survival")
	FCC_MatchPhaseChanged OnPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Survival")
	FCC_TeamStatsChanged OnTeamStatsChanged;

	/** 横幅、升级提示等（不可靠）。 */
	UPROPERTY(BlueprintAssignable, Category = "Survival")
	FCC_MatchMessageReceived OnMatchMessage;

	// --- 仅服务器 ---

	void SetPhase(ECC_MatchPhase NewPhase);
	void AddTeamStat(FGameplayTag Tag, int32 Count);
	void ResetTeamStats();

	/** 发给所有客户端（含监听服务器主机）的提示消息。 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMatchMessage(const FCC_MatchMessage& Message);

	/** 服务器便捷入口。 */
	void BroadcastMatchMessage(FGameplayTag Verb, APlayerState* Instigator = nullptr, APlayerState* Target = nullptr, float Magnitude = 0.f);

protected:
	/** GameOver 时为本地玩家打开的结算页（父类建议 CC_ActivatableWidget，Controller 选 CC_SurvivalResultController）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|UI")
	TSoftClassPtr<UCC_ActivatableWidget> ResultScreenClass;

	UPROPERTY(EditDefaultsOnly, Category = "Survival|UI", meta = (Categories = "UI.Layer"))
	FGameplayTag ResultScreenLayer;

private:
	UFUNCTION()
	void OnRep_Phase(ECC_MatchPhase OldPhase);
	UFUNCTION()
	void OnRep_TeamStats();

	void HandlePhaseChanged();
	/** 为所有本地玩家打开结算页；根布局未就绪时稍后重试。 */
	void OpenResultScreenForLocalPlayers();

	UPROPERTY(VisibleAnywhere, Category = "Survival")
	TObjectPtr<UCC_WaveDirectorComponent> WaveDirector;

	UPROPERTY(VisibleAnywhere, Category = "Survival")
	TObjectPtr<UCC_TeamProgressionComponent> Progression;

	UPROPERTY(ReplicatedUsing = OnRep_Phase)
	ECC_MatchPhase Phase = ECC_MatchPhase::WaitingToStart;

public:
	/** 对局开始的服务器时间。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survival")
	double MatchStartServerTime = 0.0;

	/** 对局结束的服务器时间；进行中为 0。 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Survival")
	double MatchEndServerTime = 0.0;

private:
	UPROPERTY(ReplicatedUsing = OnRep_TeamStats)
	FCC_StatTagStackContainer TeamStats;

	FTSTicker::FDelegateHandle ResultScreenRetry;
	int32 ResultScreenAttempts = 0;
};
