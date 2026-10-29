#pragma once

#include "CoreMinimal.h"
#include "Components/GameStateComponent.h"
#include "CC_TeamProgressionComponent.generated.h"

class APlayerController;
class UCC_SurvivalModeConfig;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_TeamProgressChanged);

/**
 * 全队共享经验与等级（挂在 ACC_SurvivalGameState 上）。
 * 服务器：AddExperience 累加经验并判定升级；每升一级，给每个玩家各排一轮三选一（UCC_BuffSelectionComponent 自带排队）。
 * 复制：等级、经验、升级所需经验，供 HUD 显示经验条。
 * 不认识任何具体 Buff；抽什么由 Progression.BuffPool 决定。
 */
UCLASS(ClassGroup = (Survival))
class GAS_DEMO_API UCC_TeamProgressionComponent : public UGameStateComponent
{
	GENERATED_BODY()

public:
	UCC_TeamProgressionComponent(const FObjectInitializer& ObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Survival|Progression")
	int32 GetTeamLevel() const { return TeamLevel; }

	UFUNCTION(BlueprintPure, Category = "Survival|Progression")
	float GetTeamExperience() const { return TeamExperience; }

	UFUNCTION(BlueprintPure, Category = "Survival|Progression")
	float GetExperienceToNextLevel() const { return ExperienceToNextLevel; }

	/** 经验条比例 [0,1]。 */
	UFUNCTION(BlueprintPure, Category = "Survival|Progression")
	float GetLevelProgress() const { return ExperienceToNextLevel > 0.f ? FMath::Clamp(TeamExperience / ExperienceToNextLevel, 0.f, 1.f) : 0.f; }

	UPROPERTY(BlueprintAssignable, Category = "Survival|Progression")
	FCC_TeamProgressChanged OnProgressChanged;

	// --- 仅服务器 ---

	/** 开局：等级 1、经验 0。 */
	void StartProgression(const UCC_SurvivalModeConfig* InConfig);
	/** 停止：之后的经验忽略。 */
	void StopProgression();
	/** 加经验并处理升级。返回本次升了几级。 */
	int32 AddExperience(float Amount);
	/** 中途加入的玩家补发 TeamLevel - 1 轮三选一。 */
	void GrantCatchUpOffers(APlayerController* Player) const;

private:
	UFUNCTION()
	void OnRep_Progress();
	void NotifyChanged();
	/** 给一个玩家排 Rounds 轮三选一。 */
	void OfferRounds(APlayerController* Player, int32 Rounds) const;

	UPROPERTY(Transient)
	TObjectPtr<const UCC_SurvivalModeConfig> Config;

	bool bRunning = false;

	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	int32 TeamLevel = 1;

	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	float TeamExperience = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	float ExperienceToNextLevel = 0.f;
};
