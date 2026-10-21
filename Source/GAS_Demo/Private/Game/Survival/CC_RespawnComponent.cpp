#include "Game/Survival/CC_RespawnComponent.h"

#include "Actors/Character/CC_PlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Game/Survival/CC_SurvivalGameMode.h"
#include "Game/Survival/CC_SurvivalGameState.h"
#include "Game/Survival/CC_SurvivalModeConfig.h"
#include "Game/Survival/CC_SurvivalPlayerState.h"
#include "Game/Survival/CC_SurvivalTags.h"
#include "GAS_Demo.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UCC_RespawnComponent::UCC_RespawnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	Random.Initialize(FMath::Rand());
}

void UCC_RespawnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAll();
	Super::EndPlay(EndPlayReason);
}

void UCC_RespawnComponent::SchedulePlayer(ACC_SurvivalPlayerState* PlayerState)
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!PlayerState || !Config || !GameState)
	{
		return;
	}
	ClearPending(PlayerState);
	const float Delay = FMath::Max(0.05f, UCC_SurvivalModeConfig::ComputeRespawnDelay(
		Config->Respawn, PlayerState->GetStat(CCSurvivalTags::StatDeaths)));
	PlayerState->SetRespawnServerTime(GameState->GetServerWorldTimeSeconds() + Delay);

	FPending& Entry = Pending.Add(PlayerState);
	const TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState(PlayerState);
	World->GetTimerManager().SetTimer(Entry.CountdownTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::HandleCountdownFinished, WeakPlayerState), Delay, false);
	UE_LOG(LogGAS_Demo, Log, TEXT("[Survival] %s 将在 %.1f 秒后复活"), *PlayerState->GetPlayerName(), Delay);
}

void UCC_RespawnComponent::HandleCountdownFinished(TWeakObjectPtr<ACC_SurvivalPlayerState> WeakPlayerState)
{
	ACC_SurvivalPlayerState* PlayerState = WeakPlayerState.Get();
	FPending* Entry = PlayerState ? Pending.Find(PlayerState) : nullptr;
	if (!Entry)
	{
		return;
	}
	Entry->bCountdownDone = true;
	if (PlayerState->GetLifeState() == ECC_PlayerLifeState::Dead)
	{
		TryRevive(PlayerState, false);
		return;
	}
	// 死亡动作还没播完：等 DeathFinished；玩家没有死亡能力时由兜底计时器强制复活。
	const float Timeout = Config ? Config->Respawn.DeathFinishedTimeout : 5.f;
	GetWorld()->GetTimerManager().SetTimer(Entry->FallbackTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakPlayerState]()
		{
			if (ACC_SurvivalPlayerState* PS = WeakPlayerState.Get())
			{
				TryRevive(PS, true);
			}
		}), FMath::Max(0.05f, Timeout), false);
}

void UCC_RespawnComponent::NotifyDeathFinished(ACC_SurvivalPlayerState* PlayerState)
{
	const FPending* Entry = PlayerState ? Pending.Find(PlayerState) : nullptr;
	if (Entry && Entry->bCountdownDone)
	{
		TryRevive(PlayerState, false);
	}
}

bool UCC_RespawnComponent::ReviveNow(ACC_SurvivalPlayerState* PlayerState)
{
	if (!PlayerState || PlayerState->IsAliveInMatch())
	{
		return false;
	}
	if (!Pending.Contains(PlayerState))
	{
		Pending.Add(PlayerState);
	}
	TryRevive(PlayerState, true);
	return PlayerState->IsAliveInMatch();
}

void UCC_RespawnComponent::TryRevive(ACC_SurvivalPlayerState* PlayerState, bool bForce)
{
	ACC_SurvivalGameMode* GameMode = GetOwner<ACC_SurvivalGameMode>();
	if (!PlayerState || !GameMode || !GameMode->IsMatchPlaying())
	{
		ClearPending(PlayerState);
		return;
	}
	if (!bForce && PlayerState->GetLifeState() != ECC_PlayerLifeState::Dead)
	{
		return;
	}
	ACC_PlayerCharacter* Pawn = Cast<ACC_PlayerCharacter>(PlayerState->GetPawn());
	ACC_PlayerCharacter* Anchor = PickAnchor(PlayerState);
	if (!Pawn || !Anchor)
	{
		// 没有 Pawn（断线中）或没有存活队友（此时 GameMode 已结算）：放弃本次复活。
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Survival] %s 无法复活：%s"), *PlayerState->GetPlayerName(),
			!Pawn ? TEXT("没有 Pawn") : TEXT("没有存活队友"));
		ClearPending(PlayerState);
		return;
	}
	const FVector Location = FindReviveLocation(Anchor, Pawn);
	ClearPending(PlayerState);
	if (Pawn->Revive(Location, Config ? Config->Respawn.ReviveEffect : nullptr))
	{
		GameMode->HandlePlayerRevived(PlayerState);
	}
}

ACC_PlayerCharacter* UCC_RespawnComponent::PickAnchor(const ACC_SurvivalPlayerState* Exclude)
{
	const ACC_SurvivalGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ACC_SurvivalGameState>() : nullptr;
	if (!GameState)
	{
		return nullptr;
	}
	TArray<ACC_PlayerCharacter*> Candidates;
	for (ACC_SurvivalPlayerState* PS : GameState->GetSurvivalPlayers())
	{
		ACC_PlayerCharacter* Pawn = PS != Exclude ? Cast<ACC_PlayerCharacter>(PS->GetPawn()) : nullptr;
		if (Pawn && PS->IsAliveInMatch() && Pawn->IsAlive())
		{
			Candidates.Add(Pawn);
		}
	}
	return Candidates.IsEmpty() ? nullptr : Candidates[Random.RandRange(0, Candidates.Num() - 1)];
}

FVector UCC_RespawnComponent::FindReviveLocation(const ACC_PlayerCharacter* Anchor, const ACC_PlayerCharacter* Reviving) const
{
	UWorld* World = GetWorld();
	const FVector AnchorLocation = Anchor->GetActorLocation();
	if (!World)
	{
		return AnchorLocation;
	}
	const UCapsuleComponent* Capsule = Reviving ? Reviving->GetCapsuleComponent() : Anchor->GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SurvivalRevive), false);
	Params.AddIgnoredActor(Reviving);
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const float MinRadius = Config ? Config->Respawn.MinRadius : 150.f;
	const float MaxRadius = Config ? FMath::Max(Config->Respawn.MaxRadius, MinRadius) : 400.f;
	const float StartAngle = FMath::FRandRange(0.f, 360.f);

	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float Angle = FMath::DegreesToRadians(StartAngle + Index * 45.f);
		const float Distance = FMath::FRandRange(MinRadius, MaxRadius);
		FVector Candidate = AnchorLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Distance;
		if (NavSys)
		{
			FNavLocation NavLocation;
			if (!NavSys->ProjectPointToNavigation(Candidate, NavLocation, FVector(Radius * 2.f, Radius * 2.f, HalfHeight * 2.f)))
			{
				continue;
			}
			Candidate = NavLocation.Location + FVector(0.f, 0.f, HalfHeight + 2.f);
		}
		if (!World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			return Candidate;
		}
	}
	return AnchorLocation;
}

void UCC_RespawnComponent::Cancel(ACC_SurvivalPlayerState* PlayerState)
{
	ClearPending(PlayerState);
	if (PlayerState)
	{
		PlayerState->SetRespawnServerTime(0.0);
	}
}

void UCC_RespawnComponent::CancelAll()
{
	TArray<TWeakObjectPtr<ACC_SurvivalPlayerState>> Keys;
	Pending.GetKeys(Keys);
	for (const TWeakObjectPtr<ACC_SurvivalPlayerState>& Key : Keys)
	{
		Cancel(Key.Get());
	}
	Pending.Reset();
}

void UCC_RespawnComponent::ClearPending(ACC_SurvivalPlayerState* PlayerState)
{
	FPending* Entry = PlayerState ? Pending.Find(PlayerState) : nullptr;
	if (!Entry)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Entry->CountdownTimer);
		World->GetTimerManager().ClearTimer(Entry->FallbackTimer);
	}
	Pending.Remove(PlayerState);
}
