#pragma once

#include "CoreMinimal.h"
#include "Game/Survival/CC_SurvivalTypes.h"
#include "CC_SurvivalUITypes.generated.h"

/**
 * 战斗 HUD 上的生存模式数据快照。倒计时只给服务器时间点，界面每帧用
 * ACC_SurvivalGameState::GetSecondsUntilServerTime 自己算，避免每秒刷新模型。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_SurvivalHUDSnapshot
{
	GENERATED_BODY()

	/** 当前地图是否为生存模式；false 时其余字段无意义，HUD 应隐藏生存模式面板。 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") bool bActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") ECC_MatchPhase Phase = ECC_MatchPhase::WaitingToStart;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 CurrentWave = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") double NextWaveServerTime = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 AliveEnemies = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 TeamLevel = 1;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") float TeamExperience = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") float ExperienceToNextLevel = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 TeamKills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 MyKills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") ECC_PlayerLifeState LifeState = ECC_PlayerLifeState::Alive;
	/** 本地玩家复活的服务器时间点；0 表示没有在倒计时。 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") double RespawnServerTime = 0.0;

	float GetLevelProgress() const { return ExperienceToNextLevel > 0.f ? FMath::Clamp(TeamExperience / ExperienceToNextLevel, 0.f, 1.f) : 0.f; }

	bool operator==(const FCC_SurvivalHUDSnapshot& Other) const
	{
		return bActive == Other.bActive && Phase == Other.Phase && CurrentWave == Other.CurrentWave
			&& NextWaveServerTime == Other.NextWaveServerTime && AliveEnemies == Other.AliveEnemies
			&& TeamLevel == Other.TeamLevel && TeamExperience == Other.TeamExperience
			&& ExperienceToNextLevel == Other.ExperienceToNextLevel && TeamKills == Other.TeamKills
			&& MyKills == Other.MyKills && LifeState == Other.LifeState && RespawnServerTime == Other.RespawnServerTime;
	}
	bool operator!=(const FCC_SurvivalHUDSnapshot& Other) const { return !(*this == Other); }
};

/** 结算页快照。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_SurvivalResultSnapshot
{
	GENERATED_BODY()

	// 全队
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 Wave = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 TeamLevel = 1;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 TeamKills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") float SurvivalTime = 0.f;

	// 本地玩家
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 Deaths = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 Revives = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 DamageDealt = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 BuffsChosen = 0;

	// 本地最佳纪录（读档完成后有效）
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") bool bRecordLoaded = false;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 BestWave = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") int32 BestKills = 0;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") float BestSurvivalTime = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") bool bNewBestWave = false;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") bool bNewBestKills = false;
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") bool bNewBestTime = false;

	/** 是否显示"再来一局 / 返回大厅"（房主或单机）。 */
	UPROPERTY(BlueprintReadOnly, Category = "UI|Survival") bool bCanControlMatch = false;
};
