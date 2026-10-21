#pragma once

#include "CoreMinimal.h"
#include "CC_EngagementTypes.generated.h"

/**
 * 攻击者对当前目标的参与程度。只表示参与程度，兵种（近战 / 弓兵）由攻击者条目的 bRanged 单独表示。
 * 黑板键 EngagementTier 使用这个枚举。
 */
UENUM(BlueprintType)
enum class ECC_EngagementTier : uint8
{
	/** 没有目标，未加入任何围攻组。 */
	None,
	/** 所在环没有空槽位，在环外侧排队。 */
	Queue,
	/** 占有外圈（近战）或远程圈（弓兵）槽位。 */
	Surround,
	/** 持有攻击令牌并占有内圈槽位，只有近战会进入。 */
	Engaged
};

/**
 * 围攻系统参数。作为 UCC_EngagementSubsystem 的 Config 属性，
 * 在 DefaultGame.ini 的 [/Script/GAS_Demo.CC_EngagementSubsystem] 段中以 Settings=(...) 覆盖。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_EngagementSettings
{
	GENERATED_BODY()

	/** 内圈半径（厘米）：近战持令牌者站位，约等于攻击距离 + 胶囊体半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings", meta = (ClampMin = "50"))
	float InnerRadius = 180.f;

	/** 外圈半径（厘米）：近战包围者站位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings", meta = (ClampMin = "50"))
	float OuterRadius = 450.f;

	/** 槽位间距（厘米），至少为 2 × 最大胶囊体半径 + 分离余量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings", meta = (ClampMin = "20"))
	float SlotSpacing = 120.f;

	/** 槽位固定随机偏移的幅度（厘米），避免站成完美的圆。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings", meta = (ClampMin = "0"))
	float SlotJitter = 15.f;

	/** 弓兵配置缺失时远程圈的半径（厘米）；正常情况下取 (MinRange + MaxRange) / 2。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings", meta = (ClampMin = "50"))
	float RangedRadiusFallback = 650.f;

	/** 排队点在所在环外侧的距离（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings", meta = (ClampMin = "0"))
	float QueueMargin = 100.f;

	/** 槽位投影到 NavMesh 的查询范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Rings")
	FVector ProjectExtent = FVector(100.f, 100.f, 250.f);

	/** 每个被围攻目标同时可发放的令牌数；实际容量还受空闲可用内圈槽位数限制。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Tokens", meta = (ClampMin = "1"))
	int32 TokenCapacity = 4;

	/** 同一目标两次发放令牌的最小间隔（秒），让出手错开。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Tokens", meta = (ClampMin = "0"))
	float MinGrantInterval = 0.4f;

	/** 持令牌者连续无进展超过这个时间（秒）就收回令牌。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Progress", meta = (ClampMin = "0.1"))
	float StuckTimeout = 2.f;

	/** 判定真正卡住时，该内圈槽位的冷却时间（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Progress", meta = (ClampMin = "0"))
	float SlotCooldown = 3.f;

	/** 目标水平速度超过它时视为在逃跑，追击者未到达也不累计无进展时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Progress", meta = (ClampMin = "0"))
	float TargetFastSpeed = 300.f;

	/** 攻击者水平速度低于它时视为自身静止，超时才会冷却槽位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Progress", meta = (ClampMin = "0"))
	float StuckSelfSpeed = 50.f;

	/** 离槽位距离比最近记录短这么多（厘米）才算有进展。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Progress", meta = (ClampMin = "1"))
	float ProgressStep = 25.f;

	/** 离内圈槽位小于这个距离（厘米）视为已到达。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Progress", meta = (ClampMin = "1"))
	float ArriveRadius = 60.f;

	/** 每个围攻组的更新间隔（秒）。各组起点随机错开。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Update", meta = (ClampMin = "0.05"))
	float UpdateInterval = 0.25f;

	/** 分配位置移动超过这个距离（厘米）才通知控制器，避免频繁重新寻路。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Engagement|Update", meta = (ClampMin = "0"))
	float NotifyDistance = 75.f;
};

/** 子系统推送给控制器、并供 CC_FollowTarget 读取的分配结果。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_EngagementAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Engagement")
	ECC_EngagementTier Tier = ECC_EngagementTier::None;

	/** 应该走到的位置：槽位或排队点。bLocationValid 为 false 时无意义。 */
	UPROPERTY(BlueprintReadOnly, Category = "Engagement")
	FVector MoveLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Engagement")
	bool bHasToken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Engagement")
	bool bLocationValid = false;

	/**
	 * 分配编号。每次发放、归还、收回令牌或重新注册都会变化，全局唯一递增。
	 * 归还令牌时必须带上开始攻击时记录的编号，对不上的归还会被忽略。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Engagement")
	int32 Generation = 0;
};
