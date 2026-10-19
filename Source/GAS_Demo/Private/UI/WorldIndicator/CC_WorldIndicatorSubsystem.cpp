#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UCC_WorldIndicatorSubsystem* UCC_WorldIndicatorSubsystem::GetForPlayer(const APlayerController* PlayerController)
{
	const ULocalPlayer* Player = IsValid(PlayerController) ? PlayerController->GetLocalPlayer() : nullptr;
	return Player ? Player->GetSubsystem<UCC_WorldIndicatorSubsystem>() : nullptr;
}

void UCC_WorldIndicatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);
}

void UCC_WorldIndicatorSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	ClearIndicators();
	Super::Deinitialize();
}

void UCC_WorldIndicatorSubsystem::AddIndicator(UCC_WorldIndicator* Indicator)
{
	if (!IsValid(Indicator) || Indicators.Contains(Indicator)) return;
	Indicators.Add(Indicator);
	OnIndicatorAdded.Broadcast(Indicator);
}

void UCC_WorldIndicatorSubsystem::RemoveIndicator(UCC_WorldIndicator* Indicator)
{
	if (Indicator && Indicators.RemoveSingle(Indicator) > 0) OnIndicatorRemoved.Broadcast(Indicator);
}

void UCC_WorldIndicatorSubsystem::ClearIndicators()
{
	// 先整体移出再逐个通知，回调里再增删不会打乱遍历。
	const TArray<TObjectPtr<UCC_WorldIndicator>> Removed = MoveTemp(Indicators);
	Indicators.Reset();
	for (UCC_WorldIndicator* Indicator : Removed) OnIndicatorRemoved.Broadcast(Indicator);
}

void UCC_WorldIndicatorSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (World && Player && World == Player->GetWorld()) ClearIndicators();
}
