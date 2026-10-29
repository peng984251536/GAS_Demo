#include "Subsystems/CC_GameEventSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

UCC_GameEventSubsystem* UCC_GameEventSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UCC_GameEventSubsystem>() : nullptr;
}

bool UCC_GameEventSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 自动化测试创建的 Game 世界也需要它。
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
