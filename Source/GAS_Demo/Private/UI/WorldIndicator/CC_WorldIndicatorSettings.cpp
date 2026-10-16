#include "UI/WorldIndicator/CC_WorldIndicatorSettings.h"
#include "UI/HealthBar/CC_HealthBarWidget.h"
#include "UI/DamageText/CC_DamageNumberWidget.h"

UCC_WorldIndicatorSettings::UCC_WorldIndicatorSettings()
{
	DefaultHealthBarWidgetClass = UCC_HealthBarWidget::StaticClass();
	DamageNumberWidgetClass = UCC_DamageNumberWidget::StaticClass();
}
