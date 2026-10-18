#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/CrowdAgentInterface.h"
#include "CC_PlayerCrowdAgentComponent.generated.h"

/**
 * 让玩家角色注册为 Detour Crowd 的 Agent。玩家只作为移动障碍参与避让，本身不受 Crowd 控制，
 * 这样敌人会提前绕开玩家，而不是规划一条从玩家身体中间穿过的路线。
 * 只在服务器上注册（Crowd 只在服务器运行）。
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_PlayerCrowdAgentComponent : public UActorComponent, public ICrowdAgentInterface
{
	GENERATED_BODY()

public:
	UCC_PlayerCrowdAgentComponent();

	//~ ICrowdAgentInterface
	virtual FVector GetCrowdAgentLocation() const override;
	virtual FVector GetCrowdAgentVelocity() const override;
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const override;
	virtual float GetCrowdAgentMaxSpeed() const override;
	//~ End ICrowdAgentInterface

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bRegistered = false;
};
