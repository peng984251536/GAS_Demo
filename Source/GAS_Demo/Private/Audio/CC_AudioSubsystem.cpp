#include "Audio/CC_AudioSubsystem.h"
#include "Audio/CC_AudioConfig.h"
#include "Audio/CC_AudioSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "GAS_Demo.h"

namespace
{
	// 收集有效变体的下标；空项跳过。
	void CollectValid(const FCC_AudioEntry& Entry, TArray<int32>& Out)
	{
		for (int32 Index = 0; Index < Entry.Sounds.Num(); ++Index)
		{
			if (IsValid(Entry.Sounds[Index]))
			{
				Out.Add(Index);
			}
		}
	}

	float RandomIn(const FFloatInterval& Range)
	{
		return FMath::FRandRange(FMath::Min(Range.Min, Range.Max), FMath::Max(Range.Min, Range.Max));
	}
}

bool UCC_AudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UCC_AudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = UCC_AudioSettings::GetAudioConfig();
	if (!Config)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("[Audio] 未找到全局音效配置：请在 项目设置 → Game → CC Audio 指定 Default Audio Config。"));
	}
}

UCC_AudioSubsystem* UCC_AudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCC_AudioSubsystem>() : nullptr;
}

FCC_AudioPick UCC_AudioSubsystem::PickStateless(const UCC_AudioConfig* InConfig, FName Event)
{
	FCC_AudioPick Result;
	Result.Entry = InConfig ? InConfig->FindEntry(Event) : nullptr;
	if (!Result.Entry)
	{
		return Result;
	}
	TArray<int32> Valid;
	CollectValid(*Result.Entry, Valid);
	if (Valid.IsEmpty())
	{
		return Result;
	}
	Result.Sound = Result.Entry->Sounds[Valid[FMath::RandHelper(Valid.Num())]];
	Result.Volume = RandomIn(Result.Entry->VolumeRange);
	Result.Pitch = RandomIn(Result.Entry->PitchRange);
	return Result;
}

FCC_AudioPick UCC_AudioSubsystem::Pick(FName Event)
{
	FCC_AudioPick Result;
	Result.Entry = Config ? Config->FindEntry(Event) : nullptr;
	TArray<int32> Valid;
	if (Result.Entry)
	{
		CollectValid(*Result.Entry, Valid);
	}
	if (Valid.IsEmpty())
	{
		if (!Event.IsNone() && !WarnedEvents.Contains(Event))
		{
			WarnedEvents.Add(Event);
			UE_LOG(LogGAS_Demo, Warning, TEXT("[Audio] 事件 %s 没有配置可用的音效，已跳过播放。"), *Event.ToString());
		}
		return Result;
	}

	int32 Choice = FMath::RandHelper(Valid.Num());
	const int32* Last = LastVariant.Find(Event);
	if (Result.Entry->bAvoidRepeat && Last && Valid.Num() > 1 && Valid[Choice] == *Last)
	{
		// 跳到其余变体中的一个，保证不重复且仍然均匀。
		Choice = (Choice + 1 + FMath::RandHelper(Valid.Num() - 1)) % Valid.Num();
	}
	LastVariant.Add(Event, Valid[Choice]);

	Result.Sound = Result.Entry->Sounds[Valid[Choice]];
	Result.Volume = RandomIn(Result.Entry->VolumeRange);
	Result.Pitch = RandomIn(Result.Entry->PitchRange);
	return Result;
}
