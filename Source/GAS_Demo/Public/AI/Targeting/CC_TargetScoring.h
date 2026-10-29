#pragma once

#include "CoreMinimal.h"
#include "CC_TargetScoring.generated.h"

class AActor;

/**
 * 选目标评分参数（距离单位：厘米）。见 Docs/AI/AI-Combat.zh-CN.md 第 4 节。
 * 评分越小越好：
 *   Score = Distance
 *         + CrowdPenalty     × max(0, MeleeAttackers − MeleeSoftCap)   （只在自己是近战时计算）
 *         − RetaliationBonus × (候选正在打自己 ? 1 : 0)
 *         + BlockedPenalty   × (最近被候选周围的人群挡住过 ? 1 : 0)
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_TargetScoringParams
{
	GENERATED_BODY()

	/** 一个目标被几个近战攻击之后开始加拥挤惩罚。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	int32 MeleeSoftCap = 3;

	/** 每超出软上限一人，等效增加的距离。300 表示「多 1 人 ≈ 远 3 米」。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	float CrowdPenalty = 300.f;

	/** 候选正在打自己时减少的分数：让两个单位更容易互相锁定、捉对厮杀。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	float RetaliationBonus = 150.f;

	/** 最近被候选周围的人群挡住过时增加的分数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Targeting", meta = (ClampMin = "0"))
	float BlockedPenalty = 600.f;
};

/**
 * 一个已经通过有效性过滤（存活、敌对、满足编队约束）的候选。
 * 只在选目标的调用栈内使用，不保存。
 */
struct GAS_DEMO_API FCC_TargetCandidate
{
	/** 候选 Actor。评分本身不读取它，只用于调用方把结果映射回 Actor；测试中可为空。 */
	AActor* Actor = nullptr;
	/** 与自己的水平距离（厘米）。 */
	float Distance = 0.f;
	/** 正在近战攻击它的人数，不含自己。 */
	int32 MeleeAttackers = 0;
	/** 它当前的目标是不是自己。 */
	bool bTargetsSelf = false;
	/** 自己最近是否被它周围的人群挡住过。 */
	bool bBlocked = false;
};

/** 选目标的纯逻辑。不依赖 World、子系统或任何 Actor 状态，可直接做自动化测试。 */
struct GAS_DEMO_API FCC_TargetScoring
{
	/**
	 * 计算单个候选的分数，越小越好。
	 * @param Candidate 候选。
	 * @param Params 评分参数。
	 * @param bSelfIsMelee 自己是否为近战；弓兵不计算拥挤项。
	 */
	static float Score(const FCC_TargetCandidate& Candidate, const FCC_TargetScoringParams& Params, bool bSelfIsMelee);

	/**
	 * 选出分数最低的候选，并对当前目标做迟滞保护：新目标的分数必须比当前目标低 SwitchHysteresis 以上才切换。
	 * @param Candidates 候选列表。
	 * @param CurrentIndex 当前目标在列表中的下标；当前目标不在列表中（离开视野 / 失效）时为 INDEX_NONE。
	 * @param Params 评分参数。
	 * @param SwitchHysteresis 切换所需的最小分数优势。
	 * @param bSelfIsMelee 自己是否为近战。
	 * @return 选中的下标；列表为空时返回 INDEX_NONE。
	 */
	static int32 SelectBest(TConstArrayView<FCC_TargetCandidate> Candidates, int32 CurrentIndex,
		const FCC_TargetScoringParams& Params, float SwitchHysteresis, bool bSelfIsMelee);
};
