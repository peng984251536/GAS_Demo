#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/CC_EnemySpawnTypes.h"
#include "CC_EnemySpawner.generated.h"

class ACC_EnemyCharacter;
class USphereComponent;

/**
 * 放入场景的怪物生成点；生成位置位于以自身为中心的水平圆形范围内。
 * 两种用法：
 *   1. 独立使用：BeginPlay 或手动调用 SpawnEnemies 一次生成一批（原有行为）。
 *   2. 交给波次系统：勾选 bManagedByWaveDirector，由调用方用 FindSpawnLocation + SpawnEnemyAt 逐只生成。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API ACC_EnemySpawner : public AActor
{

	GENERATED_BODY()

public:
	ACC_EnemySpawner();
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 选择 BP_CC_Enemy02 或 BP_Enemy_Warrior 等怪物蓝图。被波次系统接管时不使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	TSubclassOf<ACC_EnemyCharacter> EnemyClass;

	/** 每次调用尝试生成的数量。空间不足时可能少于此值。被波次系统接管时不使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner", meta=(ClampMin="0", UIMin="0"))
	int32 SpawnCount = 5;

	/** 世界空间半径，单位厘米；0 表示只在生成点处尝试。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner", meta=(ClampMin="0", UIMin="0", Units="cm"))
	float SpawnRadius = 500.0f;

	/** 开始游戏时自动生成一批。bManagedByWaveDirector 为 true 时忽略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	bool bSpawnOnBeginPlay = true;

	/**
	 * 交给波次系统（如生存模式）调度：不再自动生成，刷什么、刷多少由调用方决定。
	 * 未勾选的刷怪器不会被波次系统使用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	bool bManagedByWaveDirector = false;

	/** 从候选点上方到下方各搜索此距离，寻找静态地面。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Spawner", meta=(ClampMin="1", Units="cm"))
	float GroundSearchDistance = 1000.0f;

	/** 每只怪物的最大随机位置尝试次数，避免空间不足时无限循环。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Spawner", meta=(ClampMin="1", ClampMax="100"))
	int32 MaxAttemptsPerEnemy = 10;

	/** 仅在游戏中、服务器执行。返回本次实际生成数量；再次调用会再生成一批。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spawner")
	int32 SpawnEnemies();

	/**
	 * 在范围内找一个可生成该怪物的位置（服务器）：命中可行走的静态地面、胶囊处无阻挡，
	 * bProjectToNavigation 时还要求能投影到导航网格（没有导航系统时不做该检查）。
	 * @param Class 怪物类，用它的胶囊尺寸和可行走坡度。
	 * @param bProjectToNavigation 是否要求落点在导航网格上。
	 * @param Random 调用方的随机流，便于用种子复现。
	 * @param OutLocation 胶囊中心位置。
	 * @return 在 MaxAttemptsPerEnemy 次尝试内找到时返回 true。
	 */
	bool FindSpawnLocation(TSubclassOf<ACC_EnemyCharacter> Class, bool bProjectToNavigation,
		FRandomStream& Random, FVector& OutLocation) const;

	/**
	 * 在指定位置生成一只怪物（服务器），在 FinishSpawning 前写入 Params。
	 * 位置被占用时尝试就近调整，仍不行则放弃。
	 * @return 生成成功的怪物；失败返回空。
	 */
	ACC_EnemyCharacter* SpawnEnemyAt(TSubclassOf<ACC_EnemyCharacter> Class, const FVector& Location,
		const FCC_EnemySpawnParams& Params);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner")
	TObjectPtr<USphereComponent> SpawnArea;

private:
	bool CanSpawnHere() const;
	bool bIsSpawning = false;
};
