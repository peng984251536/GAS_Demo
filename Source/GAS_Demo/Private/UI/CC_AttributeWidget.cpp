#include "UI/CC_AttributeWidget.h"
#include "UI/CC_WidgetComponent.h"




// void UCC_AttributeWidget::SetWidgetComponent(UCC_WidgetComponent* InWidgetComponent)
// {
// 	WidgetComponent = InWidgetComponent;
//
// 	if(!IsValid(WidgetComponent))
// 	{
// 		return;
// 	}
//
// 	const float Value = WidgetComponent->GetAttributeValue(Attribute);
// 	const float MaxValue = WidgetComponent->GetAttributeValue(MaxAttribute);
// 	OnAttributeChanged(Value, MaxValue);
// 	
// }

bool UCC_AttributeWidget::MatchesAttributes(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair) const
{
	return (Attribute == Pair.Key) && (MaxAttribute == Pair.Value);
}

void UCC_AttributeWidget::OnAttributeChange(const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair,
	UCC_AttributeSet* AttributeSet)
{
	if (!IsValid(AttributeSet) || !Pair.Key.IsValid() || !Pair.Value.IsValid()) return;
	const float AttributeValue = Pair.Key.GetNumericValue(AttributeSet);
	const float MaxAttributeValue = Pair.Value.GetNumericValue(AttributeSet);

	BP_OnAttributeChange(AttributeValue,MaxAttributeValue);
}











