#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetActor.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "CC_TargetActor_GroundPlacement.generated.h"

class ACC_GroundPlacementIndicator;

/**
 * 通用"鼠标在地面选点"Target Actor，配合 UAbilityTask_WaitTargetData（UserConfirmed）使用。
 *
 * - 只在本地控制的客户端生成（不复制、不在服务器上产出数据），鼠标射线打到地面后每帧刷新落点，
 *   并驱动 ACC_GroundPlacementIndicator 显示绿（可放置）/ 红（不可放置）。
 * - 超出 MaxRange 时把落点拉回范围边缘，而不是直接判无效，手感更顺。
 * - 可放置判断（客户端，仅用于显示和拦截无效点击）：坡度、胶囊空间；有导航数据时还要求能投影到 NavMesh。
 *   服务器收到落点后必须自己再校验一次，不能信任客户端。
 * - 确认：ASC->LocalInputConfirm()（PlayerController 在瞄准状态下把攻击键转成确认）。
 *   落点无效时忽略这次确认并重新挂上确认回调。
 * - 取消：ASC->LocalInputCancel()，或轮询 CancelKeys（默认右键 / Esc）。
 * 产出 FGameplayAbilityTargetData_LocationInfo，终点为落点（地面坐标）。
 */
UCLASS(Blueprintable, NotPlaceable)
class GAS_DEMO_API ACC_TargetActor_GroundPlacement : public AGameplayAbilityTargetActor
{
	GENERATED_BODY()

public:
	ACC_TargetActor_GroundPlacement();

	/** 施放者到落点的最大水平距离（厘米）；不大于 0 表示不限。由能力在 FinishSpawningActor 前写入。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ExposeOnSpawn = true))
	float MaxRange = 1200.f;

	/** 检查落点空间用的胶囊半径（厘米），通常取召唤物胶囊；不大于 0 时跳过空间检查。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ExposeOnSpawn = true))
	float CapsuleRadius = 34.f;

	/** 检查落点空间用的胶囊半高（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ExposeOnSpawn = true))
	float CapsuleHalfHeight = 88.f;

	/** 空间检查用的碰撞通道；与召唤物胶囊阻挡的东西（墙、其他角色）重叠时不可放置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TEnumAsByte<ECollisionChannel> RoomCheckChannel = ECC_Pawn;

	/** 视为"地面"的对象类型；鼠标射线只打这些类型，因此会穿过角色。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<TEnumAsByte<EObjectTypeQuery>> GroundObjectTypes;

	/** 地面法线 Z 的最小值（坡度余弦），低于它视为墙面或陡坡。0.7 ≈ 45°。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0", ClampMax = "1"))
	float MinGroundNormalZ = 0.7f;

	/** 有导航数据时（服务器 / 单机）要求落点能投影到 NavMesh 的范围。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FVector NavProjectExtent = FVector(100.f, 100.f, 250.f);

	/** 鼠标射线长度（厘米）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "100"))
	float TraceDistance = 100000.f;

	/** 瞄准期间按下任一键即取消。为空则只能通过 ASC->LocalInputCancel 取消。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement|Input")
	TArray<FKey> CancelKeys;

	/** 瞄准期间强制显示鼠标指针，结束后恢复原状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement|Input")
	bool bShowMouseCursorWhileTargeting = true;

	/** 地面指示器类；为空时不显示指示器（仍可选点）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement|Look", meta = (ExposeOnSpawn = true))
	TSubclassOf<ACC_GroundPlacementIndicator> IndicatorClass;

	/** 指示圈半径（厘米）；不大于 0 时使用 CapsuleRadius。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement|Look", meta = (ExposeOnSpawn = true))
	float IndicatorRadius = 0.f;

	/** 当前是否有可放置的落点。 */
	UFUNCTION(BlueprintPure, Category = "Placement")
	bool HasValidPlacement() const { return bValidPlacement; }

	/** 最近一次计算出的落点（地面坐标）。 */
	UFUNCTION(BlueprintPure, Category = "Placement")
	FVector GetPlacementLocation() const { return PlacementLocation; }

	virtual void StartTargeting(UGameplayAbility* Ability) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsConfirmTargetingAllowed() override;
	virtual void ConfirmTargeting() override;
	virtual void ConfirmTargetingAndContinue() override;
	virtual void CancelTargeting() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/**
	 * 由鼠标射线计算落点。
	 * @param OutLocation 地面落点（超出 MaxRange 时为拉回后的点）。
	 * @param bOutValid 该点能否放置。
	 * @return 鼠标下没有地面时返回 false，此时指示器隐藏。
	 */
	bool ComputePlacement(FVector& OutLocation, bool& bOutValid) const;

	/** 落点处是否有足够空间、能否投影到导航网格。 */
	bool IsLocationPlaceable(const FVector& GroundLocation) const;

	FGameplayAbilityTargetDataHandle MakePlacementTargetData() const;

private:
	/** ASC::LocalInputConfirm 广播前会清空回调；拒绝确认时需要重新挂上，否则下一次点击没有反应。 */
	void RebindLocalConfirm();
	void SetCursorOverride(bool bEnable);

	UPROPERTY(Transient)
	TObjectPtr<ACC_GroundPlacementIndicator> Indicator;

	FVector PlacementLocation = FVector::ZeroVector;
	bool bValidPlacement = false;
	bool bCursorOverridden = false;
	bool bSavedShowMouseCursor = false;
};
