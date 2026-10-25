#pragma once

#include "CoreMinimal.h"
#include "CC_SquadTypes.generated.h"

class AActor;

/**
 * 编队命令（编队层，见 Docs/AI-Combat.zh-CN.md 第 6 节）。
 * 已实现：Free / Charge（自由交战）、Follow（跟随锚点 + 栓绳）。Hold / Retreat 仍按 Free 处理。
 */
UENUM(BlueprintType)
enum class ECC_SquadOrder : uint8
{
	/** 默认：自由交战。 */
	Free,
	/** 跟随锚点：没有目标时回到锚点身边；只打锚点附近的敌人，追远了会放弃目标回来。 */
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

	/** 跟随 / 坚守时，离锚点多远以内的敌人才会被选为新目标（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad", meta = (ClampMin = "0"))
	float EngageRadius = 800.f;

	/**
	 * 跟随时，当前目标离锚点超过这个距离就放弃（厘米）。
	 * 比 EngageRadius 大，目标在边界附近走动时不会反复丢失、重选。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad", meta = (ClampMin = "0"))
	float LeashRadius = 1200.f;

	/** 跟随时，自己离锚点超过这个距离就放弃一切目标、先回到锚点身边（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Squad", meta = (ClampMin = "0"))
	float ReturnRadius = 1500.f;
};

/** 编队规则的纯逻辑，不依赖 World 或 Actor，可直接做自动化测试。 */
struct GAS_DEMO_API FCC_SquadRules
{
	/**
	 * 跟随命令下的栓绳：候选能否作为目标。
	 *   - 自己离锚点超过 ReturnRadius：不接受任何目标（先回来）；
	 *   - 候选是当前目标：离锚点不超过 LeashRadius 就继续打；
	 *   - 新候选：离锚点不超过 EngageRadius 才接受。
	 * 距离都是水平距离。
	 */
	static bool PassesFollowLeash(const FCC_SquadOrder& Order, const FVector& AnchorLocation,
		const FVector& SelfLocation, const FVector& CandidateLocation, bool bIsCurrentTarget);
};
