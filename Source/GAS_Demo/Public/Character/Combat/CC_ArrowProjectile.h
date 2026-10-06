#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CC_ArrowProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** 服务器生成并判定命中的箭；位置复制到客户端，客户端不自行造成伤害。 */
UCLASS()
class GAS_DEMO_API ACC_ArrowProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACC_ArrowProjectile();

	/** 在 SpawnActorDeferred 与 FinishSpawning 之间调用，保证碰撞开始前伤害参数已就绪。 */
	void InitializeArrow(float InSpeed, float InFlatDamage);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 在游戏视口绘制箭实际使用的球形碰撞；可在投射物蓝图中关闭。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Arrow|Debug")
	bool bDrawCollisionDebug = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
	TObjectPtr<UStaticMeshComponent> ArrowMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void OnArrowHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	float FlatDamage = 10.0f;
};
