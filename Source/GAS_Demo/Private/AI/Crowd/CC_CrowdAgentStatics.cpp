#include "AI/Crowd/CC_CrowdAgentStatics.h"

#include "AIController.h"
#include "GAS_Demo.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"

namespace
{
	UCrowdFollowingComponent* GetCrowdComponent(AAIController* Controller)
	{
		return IsValid(Controller) ? Cast<UCrowdFollowingComponent>(Controller->GetPathFollowingComponent()) : nullptr;
	}
}

void UCC_CrowdAgentStatics::ConfigureCrowdAgent(AAIController* Controller)
{
	UCrowdFollowingComponent* Crowd = GetCrowdComponent(Controller);
	if (!Crowd)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[AI][Crowd] %s has no UCrowdFollowingComponent; avoidance disabled."),
			*GetNameSafe(Controller));
		return;
	}
	Crowd->SetCrowdSeparation(true);
	Crowd->SetCrowdSeparationWeight(2.f);
	Crowd->SetCrowdAnticipateTurns(true);
	Crowd->SetCrowdCollisionQueryRange(600.f);
	Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
}

int32 UCC_CrowdAgentStatics::UpdateAvoidanceLOD(AAIController* Controller, float DistanceToNearestPlayer,
	int32 CurrentLOD, float HighDistance, float MediumDistance)
{
	UCrowdFollowingComponent* Crowd = GetCrowdComponent(Controller);
	if (!Crowd)
	{
		return CurrentLOD;
	}
	const int32 NewLOD = DistanceToNearestPlayer < HighDistance ? 2
		: (DistanceToNearestPlayer < MediumDistance ? 1 : 0);
	if (NewLOD == CurrentLOD)
	{
		return CurrentLOD;
	}
	switch (NewLOD)
	{
	case 2:
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::High);
		break;
	case 1:
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Medium);
		break;
	default:
		Crowd->SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Low);
		break;
	}
	UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Crowd] %s avoidance LOD %d -> %d (distance %.0f)"),
		*GetNameSafe(Controller), CurrentLOD, NewLOD, DistanceToNearestPlayer);
	return NewLOD;
}

void UCC_CrowdAgentStatics::SetAttacking(AAIController* Controller, bool bAttacking)
{
	UCrowdFollowingComponent* Crowd = GetCrowdComponent(Controller);
	if (!Crowd)
	{
		return;
	}
	if (bAttacking && Crowd->GetStatus() != EPathFollowingStatus::Idle)
	{
		// 攻击期间本来就不应移动；不停下来引擎会拒绝切换状态。
		Controller->StopMovement();
	}
	Crowd->SetCrowdSimulationState(bAttacking ? ECrowdSimulationState::ObstacleOnly : ECrowdSimulationState::Enabled);
	UE_LOG(LogGAS_Demo, Verbose, TEXT("[AI][Crowd] %s attacking=%d"), *GetNameSafe(Controller), bAttacking);
}
