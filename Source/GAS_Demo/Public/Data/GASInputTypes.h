// GASInputTypes.h

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "GASInputTypes.generated.h"

// 管理输入映射的枚举
UENUM(BlueprintType)
enum class EGASInputMappingType : uint8
{
	Player,
	UI,
	Combat,
	Vehicle,
	Debug
};


// 输入映射结构体
USTRUCT(BlueprintType)
struct FGASInputMappingContextEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Priority = 0;
};














