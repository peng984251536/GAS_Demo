#include "Game/Survival/CC_TeamProgressionComponent.h"

#include "Components/CC_BuffSelectionComponent.h"
#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalModeConfig.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "GameFramework/PlayerController.h"
#include "GAS_Demo.h"
#include "Net/UnrealNetwork.h"

UCC_TeamProgressionComponent::UCC_TeamProgressionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UCC_TeamProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCC_TeamProgressionComponent, TeamLevel);
	DOREPLIFETIME(UCC_TeamProgressionComponent, TeamExperience);
	DOREPLIFETIME(UCC_TeamProgressionComponent, ExperienceToNextLevel);
}

void UCC_TeamProgressionComponent::StartProgression(const UCC_SurvivalModeConfig* InConfig)
{
	if (!GetOwner()->HasAuthority() || !InConfig)
	{
		return;
	}
	Config = InConfig;
	bRunning = true;
	TeamLevel = 1;
	TeamExperience = 0.f;
	ExperienceToNextLevel = UCC_SurvivalModeConfig::ComputeExperienceToNext(Config->Progression, TeamLevel);
	NotifyChanged();
}

void UCC_TeamProgressionComponent::StopProgression()
{
	bRunning = false;
}

int32 UCC_TeamProgressionComponent::AddExperience(float Amount)
{
	if (!GetOwner()->HasAuthority() || !bRunning || !Config || Amount <= 0.f)
	{
		return 0;
	}
	const int32 OldLevel = TeamLevel;
	const int32 Gained = UCC_SurvivalModeConfig::ApplyExperience(Config->Progression, TeamLevel, TeamExperience, Amount);
	ExperienceToNextLevel = UCC_SurvivalModeConfig::ComputeExperienceToNext(Config->Progression, TeamLevel);
	NotifyChanged();

	if (Gained > 0)
	{
		UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] 全队升级 %d -> %d"), OldLevel, TeamLevel);
		// 每升一级，每个玩家（含阵亡玩家，复活后再选）各排一轮。
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			OfferRounds(It->Get(), Gained);
		}
		if (ACC_SurvivalGameState* GameState = GetGameState<ACC_SurvivalGameState>())
		{
			GameState->BroadcastMatchMessage(CCSurvivalTags::MsgLevelUp, nullptr, nullptr, static_cast<float>(TeamLevel));
		}
	}
	return Gained;
}

void UCC_TeamProgressionComponent::GrantCatchUpOffers(APlayerController* Player) const
{
	if (GetOwner()->HasAuthority() && bRunning)
	{
		OfferRounds(Player, TeamLevel - 1);
	}
}

void UCC_TeamProgressionComponent::OfferRounds(APlayerController* Player, int32 Rounds) const
{
	if (!Player || Rounds <= 0 || !Config || !Config->Progression.BuffPool)
	{
		return;
	}
	if (UCC_BuffSelectionComponent* Selection = UCC_BuffSelectionComponent::FindForPlayer(Player))
	{
		for (int32 Round = 0; Round < Rounds; ++Round)
		{
			Selection->OfferBuffs(Config->Progression.BuffPool);
		}
	}
}

void UCC_TeamProgressionComponent::NotifyChanged()
{
	GetOwner()->ForceNetUpdate();
	OnProgressChanged.Broadcast();
}

void UCC_TeamProgressionComponent::OnRep_Progress()
{
	OnProgressChanged.Broadcast();
}
