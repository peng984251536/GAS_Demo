#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "AttributeSet.h"
#include "UI/CC_WidgetComponent.h"

#include "CC_AttributeWidget.generated.h"

class UCC_WidgetComponent;

/** 普通 UMG 属性展示控件：接收当前值和最大值，蓝图负责血条、文字及局部动画。 */
UCLASS()
class GAS_DEMO_API UCC_AttributeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//void SetWidgetComponent(UCC_WidgetComponent* InWidgetComponent);
	/**
	 * 属性是否合理
	 * @param Pair 
	 * @return 
	 */
	bool MatchesAttributes(const TTuple<FGameplayAttribute,FGameplayAttribute>& Pair) const;
	/**
	 * 属性发生改变
	 * @param Pair 
	 * @param AttributeSet 
	 */
	void OnAttributeChange(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair,
							UCC_AttributeSet* AttributeSet);

protected:
	/** 当前属性，例如 Health。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attribute")
	FGameplayAttribute Attribute;

	/** 对应最大属性，例如 MaxHealth。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attribute")
	FGameplayAttribute MaxAttribute;


	/** 初次绑定、当前值变化或最大值变化时触发；蓝图计算比例时需防止除零。 */
	UFUNCTION(BlueprintImplementableEvent,meta = (DisplayName = "On Attribute Change"),Category="Attribute")
	void BP_OnAttributeChange(
		float NewValue,
		float NewMaxValue);
	
private:
	/** 旧实现保留的组件引用；当前数据由组件通过事件主动推送。 */
	UPROPERTY()
	TObjectPtr<UCC_WidgetComponent> WidgetComponent;
	
};

