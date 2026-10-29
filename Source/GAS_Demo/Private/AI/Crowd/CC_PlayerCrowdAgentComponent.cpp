#include "AI/Crowd/CC_PlayerCrowdAgentComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS_Demo.h"
#include "Navigation/CrowdManager.h"

UCC_PlayerCrowdAgentComponent::UCC_PlayerCrowdAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UCC_PlayerCrowdAgentComponent::BeginPlay()
{
	Super::BeginPlay();
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	if (UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld()))
	{
		CrowdManager->RegisterAgent(this);
		bRegistered = true;
		UE_LOG(LogGAS_Demo, Log, TEXT("[AI][Crowd] Player agent registered: %s"), *GetNameSafe(Owner));
	}
}

void UCC_PlayerCrowdAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegistered)
	{
		if (UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld()))
		{
			CrowdManager->UnregisterAgent(this);
		}
		bRegistered = false;
	}
	Super::EndPlay(EndPlayReason);
}

FVector UCC_PlayerCrowdAgentComponent::GetCrowdAgentLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}
	// Crowd 按脚底位置工作：胶囊中心向下半高。
	const ACharacter* Character = Cast<ACharacter>(Owner);
	const float HalfHeight = Character && Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.f;
	return Owner->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
}

FVector UCC_PlayerCrowdAgentComponent::GetCrowdAgentVelocity() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetVelocity() : FVector::ZeroVector;
}

void UCC_PlayerCrowdAgentComponent::GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetCapsuleComponent())
	{
		Character->GetCapsuleComponent()->GetScaledCapsuleSize(CylinderRadius, CylinderHalfHeight);
		return;
	}
	CylinderRadius = 40.f;
	CylinderHalfHeight = 90.f;
}

float UCC_PlayerCrowdAgentComponent::GetCrowdAgentMaxSpeed() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character && Character->GetCharacterMovement())
	{
		return Character->GetCharacterMovement()->GetMaxSpeed();
	}
	return 600.f;
}
