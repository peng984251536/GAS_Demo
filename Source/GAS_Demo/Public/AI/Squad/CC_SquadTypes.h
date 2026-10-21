#pragma once

#include "CoreMinimal.h"
#include "CC_SquadTypes.generated.h"

class AActor;

/**
 * 编队命令（编队层接口预留，见 Docs/AI-Combat.zh-CN.md 第 6 节）。
 * v3 只有 Free / Charge 生效（自由交战）；其余命令保存并写入黑板，行为按 Free 处理。
 */
UENUM(BlueprintType)
enum class ECC_SquadOrder : uint8
{
	/** 默认：自由交战。 */
	Free,
	/** 跟随锚点，只打交战半径内的敌人（预留）。 */
	Follow,
	/** 坚守位置（预留）。 */
	Hold,
	/** 冲锋：脱离阵型自由交战（v3 等同 Free）。 */
	Charge,
	/** 撤退到锚点，不主动交战（预留）。 */
	Retreat
};

/** 一条编队命令。由将来的 UCC_SquadComponent 下发给每个成员的 ACC_AIController::SetSquadOrder。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_SquadOrder
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad")
	ECC_SquadOrder Order = ECC_SquadOrder::Free;

	/** 跟随 / 撤退的锚点，通常是玩家。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad")
	TObjectPtr<AActor> Anchor = nullptr;

	/** 坚守位置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad")
	FVector Location = FVector::ZeroVector;

	/** 跟随 / 坚守时，离锚点多远以内的敌人才会被选为目标（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad", meta = (ClampMin = "0"))
	float EngageRadius = 800.f;
};
