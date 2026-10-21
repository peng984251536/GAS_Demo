#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CC_GroundPlacementIndicator.generated.h"

class UDecalComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * 地面放置指示器：选点类能力瞄准时贴在鼠标所指地面上的高亮圈，只在本地客户端存在，不复制。
 *
 * 两种表现，二选一：
 *   1. 配置 DecalMaterial（Material Domain = Deferred Decal）→ 用贴花投到地面，贴合起伏地形。
 *   2. 不配置 → 退化为引擎自带的扁圆盘（/Engine/BasicShapes/Cylinder），无需任何资产即可使用。
 * 两种方式都通过材质的 Vector 参数 ColorParameterName 切换"可放置 / 不可放置"颜色。
 * 需要更多表现（粒子、动画）时在蓝图子类里实现 OnPlacementValidityChanged。
 */
UCLASS(Blueprintable, NotPlaceable)
class GAS_DEMO_API ACC_GroundPlacementIndicator : public AActor
{
	GENERATED_BODY()

public:
	ACC_GroundPlacementIndicator();

	/**
	 * 设置指示圈半径。
	 * @param InRadius 半径（厘米），一般取召唤物胶囊半径或技能作用半径；小于 1 时按 1 处理。
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void SetRadius(float InRadius);

	/**
	 * 每帧由 Target Actor 调用：移动到落点并按是否可放置切换颜色。
	 * @param Location 地面落点（世界坐标）。
	 * @param bValid 该点能否放置。
	 */
	UFUNCTION(BlueprintCallable, Category = "Placement")
	void UpdatePlacement(const FVector& Location, bool bValid);

	UFUNCTION(BlueprintPure, Category = "Placement")
	bool IsPlacementValid() const { return bLastValid; }

protected:
	virtual void BeginPlay() override;

	/** 可放置 / 不可放置状态变化时触发（首次更新也会触发一次），供蓝图子类追加表现。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Placement")
	void OnPlacementValidityChanged(bool bValid);

	/** 贴花材质（Deferred Decal）。为空时使用扁圆盘。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Look")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	/** 材质里控制颜色的 Vector 参数名。引擎 BasicShapeMaterial 的参数名就是 Color。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Look")
	FName ColorParameterName = TEXT("Color");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Look")
	FLinearColor ValidColor = FLinearColor(0.1f, 1.f, 0.35f, 1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Look")
	FLinearColor InvalidColor = FLinearColor(1.f, 0.12f, 0.08f, 1.f);

	/** 贴花向下投射的深度（厘米），地形起伏大时调大。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Look", meta = (ClampMin = "1"))
	float DecalDepth = 200.f;

	/** 扁圆盘离地高度（厘米），避免与地面 Z-fighting。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement|Look", meta = (ClampMin = "0"))
	float DiscHeightOffset = 2.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDecalComponent> Decal;

	/** 没有贴花材质时使用；可在蓝图子类里换成别的网格（如 TopDown 模板的 SM_CursorMesh）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Disc;

private:
	void ApplyRadius();
	void ApplyColor(bool bValid);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ColorMaterial;

	float Radius = 50.f;
	bool bLastValid = false;
	bool bHasValidity = false;
};
