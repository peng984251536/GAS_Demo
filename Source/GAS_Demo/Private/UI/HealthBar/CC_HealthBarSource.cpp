#include "UI/HealthBar/CC_HealthBarSource.h"
#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSettings.h"
#include "UI/WorldIndicator/CC_WorldIndicatorSubsystem.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Data/CC_CharacterConfig.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

void UCC_HealthBarSource::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UCC_WorldIndicatorSubsystem>();
	Super::Initialize(Collection);
	ReadyHandle = ACC_BaseCharacter::OnAnyCharacterReady.AddUObject(this, &ThisClass::AddHealthBar);
}

void UCC_HealthBarSource::Deinitialize()
{
	ACC_BaseCharacter::OnAnyCharacterReady.Remove(ReadyHandle);
	TArray<TWeakObjectPtr<AActor>> Actors;
	HealthBars.GetKeys(Actors);
	for (const TWeakObjectPtr<AActor>& Actor : Actors) RemoveHealthBar(Actor.Get());
	Super::Deinitialize();
}

void UCC_HealthBarSource::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	ScanWorld();
}

void UCC_HealthBarSource::ScanWorld()
{
	UWorld* World = GetLocalPlayer() ? GetLocalPlayer()->GetWorld() : nullptr;
	if (!World) return;
	for (TActorIterator<ACC_BaseCharacter> It(World); It; ++It)
		if (It->IsASCInitialized()) AddHealthBar(*It);
}

void UCC_HealthBarSource::AddHealthBar(ACC_BaseCharacter* Character)
{
	const ULocalPlayer* Player = GetLocalPlayer();
	if (!IsValid(Character) || !Player || Character->GetWorld() != Player->GetWorld() || HealthBars.Contains(Character)) return;
	// 自己的角色不显示：用 PlayerState 判断，客户端复制早期 Controller 可能还没设置。
	const APlayerController* PC = Player->PlayerController;
	if (PC && Character->GetPlayerState() && Character->GetPlayerState() == PC->PlayerState) return;
	const UCC_CharacterConfig* Config = Character->GetCharacterConfig();
	if (Config && !Config->bShowOverheadHealthBar) return;
	UCC_WorldIndicatorSubsystem* Indicators = Player->GetSubsystem<UCC_WorldIndicatorSubsystem>();
	if (!Indicators) return;

	// 未配置角色配置的角色也按默认样式显示，避免漏配时没有血条。
	const FCC_OverheadHealthBarConfig BarConfig = Config ? Config->OverheadHealthBar : FCC_OverheadHealthBarConfig();
	UCC_WorldIndicator* Indicator = NewObject<UCC_WorldIndicator>(this);
	Indicator->SetAnchorActor(Character);
	Indicator->WorldOffset = BarConfig.WorldOffset;
	Indicator->MaxDistance = BarConfig.MaxDistance;
	Indicator->Alignment = FVector2D(0.5f, 1.f);
	Indicator->Priority = 0;
	Indicator->WidgetClass = BarConfig.WidgetClass.IsNull()
		? GetDefault<UCC_WorldIndicatorSettings>()->DefaultHealthBarWidgetClass : BarConfig.WidgetClass;
	HealthBars.Add(Character, Indicator);
	Character->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleCharacterEndPlay);
	Indicators->AddIndicator(Indicator);
}

void UCC_HealthBarSource::HandleCharacterEndPlay(AActor* Actor, EEndPlayReason::Type Reason) { RemoveHealthBar(Actor); }

void UCC_HealthBarSource::RemoveHealthBar(AActor* Actor)
{
	TWeakObjectPtr<UCC_WorldIndicator> Indicator;
	if (!Actor || !HealthBars.RemoveAndCopyValue(Actor, Indicator)) return;
	if (IsValid(Actor)) Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleCharacterEndPlay);
	if (const ULocalPlayer* Player = GetLocalPlayer())
		if (UCC_WorldIndicatorSubsystem* Indicators = Player->GetSubsystem<UCC_WorldIndicatorSubsystem>())
			Indicators->RemoveIndicator(Indicator.Get());
}
