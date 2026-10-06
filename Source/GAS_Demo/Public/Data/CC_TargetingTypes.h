#pragma once

#include "CoreMinimal.h"
#include "CC_TargetingTypes.generated.h"

class AActor;

/** 一次目标查询的结果。保留原反射名称和字段名称，兼容已有蓝图节点。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FClosestActorWithTagResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Targeting")
	TWeakObjectPtr<AActor> Actor;

	/** 查询时的距离快照，不会随双方移动自动更新；先检查 Actor 是否有效。 */
	UPROPERTY(BlueprintReadWrite, Category = "Targeting")
	FVector LastKnownLocation = FVector::ZeroVector;

	FClosestActorWithTagResult()
	{
	}
	FClosestActorWithTagResult(AActor* _actor,FVector _LastKnownLocation)
	{
		Actor = _actor;
		LastKnownLocation = _LastKnownLocation;
	}
	
	bool HasValidActor() const { return Actor.IsValid(); }
	void Reset()
	{
		Actor.Reset();
		LastKnownLocation = FVector::ZeroVector;
	}
};
