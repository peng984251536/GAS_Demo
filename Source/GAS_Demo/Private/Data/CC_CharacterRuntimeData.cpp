#include "Data/CC_CharacterRuntimeData.h"

#include "Net/UnrealNetwork.h"

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"
#endif

void UCC_CharacterRuntimeData::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	
	DOREPLIFETIME(ThisClass, bAlive);
	DOREPLIFETIME(ThisClass, bHit);
	DOREPLIFETIME_CONDITION(ThisClass, LastMoveInputDirection, COND_SkipOwner);
}

#if UE_WITH_IRIS
void UCC_CharacterRuntimeData::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context,
	UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	UE::Net::FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}
#endif

void UCC_CharacterRuntimeData::SetLastMoveInputDirection(const FVector& InDirection)
{
	LastMoveInputDirection = InDirection.GetSafeNormal2D();
}

void UCC_CharacterRuntimeData::SetClosestActor(const FClosestActorWithTagResult& InTarget)
{
	//ClosestActor = InTarget.HasValidActor() ? InTarget : FClosestActorWithTagResult{};
	ClosestActor = InTarget;
}

void UCC_CharacterRuntimeData::ResetForRespawn()
{
	bAlive = true;
	bHit = false;
	LastMoveInputDirection = FVector::ForwardVector;
	ClosestActor.Reset();
}
