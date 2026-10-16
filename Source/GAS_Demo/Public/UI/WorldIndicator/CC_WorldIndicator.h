// 世界指示器：一个"摆在世界某处的屏幕 UI"的数据。血条、伤害数字、交互提示等共用。
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "CC_WorldIndicator.generated.h"

class UUserWidget;

/**
 * 一个世界指示器：放在哪里、用哪个控件显示。
 *
 * 只放所有类型都通用的字段；某一类 UI 专有的数据（伤害数值、交互按键等）放在派生类里，
 * 控件在 BindIndicator 时 Cast 成对应派生类读取。
 *
 * 由数据来源创建并交给 UCC_WorldIndicatorSubsystem::AddIndicator；之后修改字段会在下一帧生效。
 */
UCLASS(BlueprintType, Blueprintable)
class GAS_DEMO_API UCC_WorldIndicator : public UObject
{
	GENERATED_BODY()
public:
	/** 跟随的 Actor；为空时使用 WorldLocation 作为固定坐标。弱引用，不延长 Actor 生命。 */
	UPROPERTY()
	TWeakObjectPtr<AActor> AnchorActor;

	UFUNCTION(BlueprintCallable, Category="World Indicator")
	void SetAnchorActor(AActor* Actor) { AnchorActor = Actor; }
	UFUNCTION(BlueprintPure, Category="World Indicator")
	AActor* GetAnchorActor() const { return AnchorActor.Get(); }

	/** 不跟随 Actor 时的固定世界坐标（厘米）。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	FVector WorldLocation = FVector::ZeroVector;

	/** 叠加在锚点上的世界偏移（厘米），例如头顶高度；不随 Actor 旋转。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	FVector WorldOffset = FVector::ZeroVector;

	/** 投影后的屏幕偏移（HUD 布局单位），负 Y 向上。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	FVector2D ScreenOffset = FVector2D::ZeroVector;

	/** 控件哪一点对齐锚点：(0.5, 1) 为底边中点，(0.5, 0.5) 为中心。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	FVector2D Alignment = FVector2D(0.5f, 1.f);

	/** 超过这个距离（世界厘米）不显示；0 表示不限。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	float MaxDistance = 0.f;

	/** 叠放顺序，数值大的画在上面（如伤害数字高于血条）。添加后修改不生效。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	int32 Priority = 0;

	/** 显示用的控件，必须实现 CC World Indicator Widget 接口；软引用，专用服务器不会加载。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator", meta=(MustImplement="/Script/GAS_Demo.CC_WorldIndicatorWidget"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** false 时暂时隐藏，不移除。 */
	UPROPERTY(BlueprintReadWrite, Category="World Indicator")
	bool bVisible = true;

	/** 当前锚点的世界坐标（含 WorldOffset）。跟随的 Actor 已失效时返回 false。 */
	bool GetAnchorLocation(FVector& OutLocation) const;
	/** 跟随 Actor 但它已经失效。 */
	bool IsAnchorLost() const { return !AnchorActor.IsExplicitlyNull() && !AnchorActor.IsValid(); }
};
