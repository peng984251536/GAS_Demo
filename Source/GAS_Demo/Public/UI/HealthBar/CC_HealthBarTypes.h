// 头顶血条的配置与展示数据。单独成文件，供角色配置（CC_CharacterConfig）引用而不依赖血条子系统。
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CC_HealthBarTypes.generated.h"

/** 某个角色的头顶血条配置：用哪个单条控件、挂在哪、显示规则。每个角色只保存数据，不创建组件。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_HealthBarOptions
{
	GENERATED_BODY()
	/**
	 * 单条血条控件，必须实现 CC_HealthBarItem 接口；可以是任意 UserWidget 蓝图。
	 * 为空时使用血条层的 Default Item Widget Class（默认是原生的 CC_DefaultHealthBarItemWidget）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar", meta=(MustImplement="/Script/GAS_Demo.CC_HealthBarItem"))
	TSubclassOf<UUserWidget> ItemWidgetClass;
	/** 在角色位置上叠加的世界空间偏移，单位厘米；不随角色旋转，用于把锚点移到头顶。控件底边中点对齐这个锚点。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FVector WorldOffset = FVector(0, 0, 80);
	/** 建议尺寸，HUD 布局单位；默认控件按它设置宽高，自定义控件可以读取也可以忽略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FVector2D Size = FVector2D(90, 8);
	/** 建议填充颜色；默认控件使用它，自定义控件可以读取也可以忽略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FLinearColor Color = FLinearColor(0.25f, 0.8f, 0.3f);
	/** 最远显示距离，单位为世界厘米；0 表示不按距离剔除。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar", meta=(ClampMin="0")) float MaxDistance = 5000;
	/** 生命值达到最大值时隐藏，仍保留数据与订阅。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bHideWhenFull = false;
	/** 生命值小于等于 0 时隐藏；隐藏不等于移除条目。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bHideWhenDead = true;
	/** 开启后自动订阅角色 ASC 的 Health / MaxHealth；未就绪时定期重试。关闭后由蓝图手动推送数值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bUseGAS = true;
};

/** 每帧推送给单条血条控件的数据。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_HealthBarItemData
{
	GENERATED_BODY()
	/** 已平滑插值的显示比例，0..1；直接用于进度条即可。 */
	UPROPERTY(BlueprintReadOnly, Category="Health Bar") float Fraction = 0.f;
	/** 当前生命值（未插值），用于显示数字。 */
	UPROPERTY(BlueprintReadOnly, Category="Health Bar") float Health = 0.f;
	/** 最大生命值。 */
	UPROPERTY(BlueprintReadOnly, Category="Health Bar") float MaxHealth = 0.f;
	/** 配置中的建议尺寸。 */
	UPROPERTY(BlueprintReadOnly, Category="Health Bar") FVector2D Size = FVector2D::ZeroVector;
	/** 配置中的建议颜色。 */
	UPROPERTY(BlueprintReadOnly, Category="Health Bar") FLinearColor Color = FLinearColor::White;

	bool operator==(const FCC_HealthBarItemData& Other) const
	{
		return Fraction == Other.Fraction && Health == Other.Health && MaxHealth == Other.MaxHealth && Size == Other.Size && Color == Other.Color;
	}
	bool operator!=(const FCC_HealthBarItemData& Other) const { return !(*this == Other); }
};
