#include "Audio/CC_AudioLibrary.h"
#include "Audio/CC_AudioSettings.h"
#include "Audio/CC_AudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// 有子系统时按事件规则取音（含避免重复）；动画编辑器预览等没有 GameInstance 时退回无状态取音。
	FCC_AudioPick PickFor(const UObject* WorldContextObject, FName Event)
	{
		if (UCC_AudioSubsystem* Subsystem = UCC_AudioSubsystem::Get(WorldContextObject))
		{
			return Subsystem->Pick(Event);
		}
		return UCC_AudioSubsystem::PickStateless(UCC_AudioSettings::GetAudioConfig(), Event);
	}

	// 专用服务器、无效 World 不播放。
	bool CanPlay(const UObject* WorldContextObject)
	{
		const UWorld* World = GEngine
			? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World && World->GetNetMode() != NM_DedicatedServer;
	}

	USoundAttenuation* AttenuationFor(const FCC_AudioPick& Pick)
	{
		if (Pick.Entry && Pick.Entry->Attenuation)
		{
			return Pick.Entry->Attenuation;
		}
		const UCC_AudioConfig* Config = UCC_AudioSettings::GetAudioConfig();
		return Config ? Config->DefaultAttenuation.Get() : nullptr;
	}

	USoundConcurrency* ConcurrencyFor(const FCC_AudioPick& Pick)
	{
		return Pick.Entry ? Pick.Entry->Concurrency.Get() : nullptr;
	}
}

void UCC_AudioLibrary::PlayAudio2D(const UObject* WorldContextObject, FName Event, float VolumeScale)
{
	if (!CanPlay(WorldContextObject))
	{
		return;
	}
	const FCC_AudioPick Pick = PickFor(WorldContextObject, Event);
	if (Pick.Sound)
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, Pick.Sound, Pick.Volume * VolumeScale, Pick.Pitch, 0.f,
			ConcurrencyFor(Pick), nullptr, /*bIsUISound*/ true);
	}
}

void UCC_AudioLibrary::PlayAudioAtLocation(const UObject* WorldContextObject, FName Event, FVector Location,
	float VolumeScale)
{
	if (!CanPlay(WorldContextObject))
	{
		return;
	}
	const FCC_AudioPick Pick = PickFor(WorldContextObject, Event);
	if (Pick.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Pick.Sound, Location, FRotator::ZeroRotator,
			Pick.Volume * VolumeScale, Pick.Pitch, 0.f, AttenuationFor(Pick), ConcurrencyFor(Pick));
	}
}

UAudioComponent* UCC_AudioLibrary::PlayAudioAttached(FName Event, USceneComponent* AttachTo, FName SocketName,
	float VolumeScale)
{
	if (!IsValid(AttachTo) || !CanPlay(AttachTo))
	{
		return nullptr;
	}
	const FCC_AudioPick Pick = PickFor(AttachTo, Event);
	if (!Pick.Sound)
	{
		return nullptr;
	}
	return UGameplayStatics::SpawnSoundAttached(Pick.Sound, AttachTo, SocketName, FVector::ZeroVector,
		FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed*/ false,
		Pick.Volume * VolumeScale, Pick.Pitch, 0.f, AttenuationFor(Pick), ConcurrencyFor(Pick));
}

UCC_AudioConfig* UCC_AudioLibrary::GetAudioConfig()
{
	return UCC_AudioSettings::GetAudioConfig();
}

USoundBase* UCC_AudioLibrary::PickGlobalAudioSound(FName Event)
{
	const UCC_AudioConfig* Config = GetAudioConfig();
	return Config ? Config->PickSound(Event) : nullptr;
}

TArray<FName> UCC_AudioLibrary::GetAudioEventNames()
{
	TArray<FName> Names;
	if (const UCC_AudioConfig* Config = GetAudioConfig())
	{
		Config->Events.GetKeys(Names);
		Names.Sort(FNameLexicalLess());
	}
	return Names;
}
