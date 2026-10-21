#include "UI/Character/CC_CharacterScreenModel.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

float CCCharacterScreen::GetServerTime(const UWorld* World)
{
	if (!World) return 0.f;
	const AGameStateBase* State = World->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

float CCCharacterScreen::GetRemainingTime(const UWorld* World, const FCC_CharacterBuffEntry& Buff)
{
	if (Buff.IsInfinite()) return -1.f;
	return FMath::Max(0.f, Buff.EndServerTime - GetServerTime(World));
}

void UCC_CharacterScreenModel::SetStats(const FText& InDisplayName, const FCC_PlayerVitals& InVitals, TArray<FCC_CharacterStatEntry>&& InStats)
{
	if (DisplayName.EqualTo(InDisplayName) && Vitals == InVitals && Stats == InStats) return;
	DisplayName = InDisplayName;
	Vitals = InVitals;
	Stats = MoveTemp(InStats);
	++StatsRevision;
	NotifyChanged();
}

void UCC_CharacterScreenModel::SetBuffs(TArray<FCC_CharacterBuffEntry>&& InBuffs)
{
	if (Buffs == InBuffs) return;
	Buffs = MoveTemp(InBuffs);
	++BuffsRevision;
	NotifyChanged();
}
