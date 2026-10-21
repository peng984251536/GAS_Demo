#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CC_SurvivalSaveGame.generated.h"

/**
 * 生存模式的本地最佳纪录（每台机器、每个本地玩家一份，不同步）。
 * 由结算页控制器异步读写，存档槽 SlotName，用户索引为本地玩家的 ControllerId。
 */
UCLASS()
class GAS_DEMO_API UCC_SurvivalSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;
	static const TCHAR* SlotName() { return TEXT("SurvivalRecord"); }

	/** 存档格式版本；以后字段变化时按版本迁移。 */
	UPROPERTY(SaveGame)
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Survival")
	int32 BestWave = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Survival")
	int32 BestKills = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Survival")
	float BestSurvivalTime = 0.f;
};
