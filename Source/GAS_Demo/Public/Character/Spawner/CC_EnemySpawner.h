#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CC_EnemySpawner.generated.h"

class ACC_EnemyCharacter;
class USphereComponent;

/** 放入场景的怪物生成点；生成位置位于以自身为中心的水平圆形范围内。 */
UCLASS(Blueprintable)
class GAS_DEMO_API ACC_EnemySpawner : public AActor
{

	GENERATED_BODY()

public:
	ACC_EnemySpawner();
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 选择 BP_CC_Enemy02 或 BP_Enemy_Warrior 等怪物蓝图。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	TSubclassOf<ACC_EnemyCharacter> EnemyClass;

	/** 每次调用尝试生成的数量。空间不足时可能少于此值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner", meta=(ClampMin="0", UIMin="0"))
	int32 SpawnCount = 5;

	/** 世界空间半径，单位厘米；0 表示只在生成点处尝试。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner", meta=(ClampMin="0", UIMin="0", Units="cm"))
	float SpawnRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spawner")
	bool bSpawnOnBeginPlay = true;

	/** 从候选点上方到下方各搜索此距离，寻找静态地面。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Spawner", meta=(ClampMin="1", Units="cm"))
	float GroundSearchDistance = 1000.0f;

	/** 每只怪物的最大随机位置尝试次数，避免空间不足时无限循环。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Spawner", meta=(ClampMin="1", ClampMax="100"))
	int32 MaxAttemptsPerEnemy = 10;

	/** 仅在游戏中、服务器执行。返回本次实际生成数量；再次调用会再生成一批。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spawner")
	int32 SpawnEnemies();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner")
	TObjectPtr<USphereComponent> SpawnArea;

private:
	bool bIsSpawning = false;
};
