#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CC_WorldIndicatorSettings.generated.h"

class UUserWidget;

/** 项目设置 → Game → CC World Indicator：世界空间 UI 的默认控件与显示规则。 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="CC World Indicator"))
class GAS_DEMO_API UCC_WorldIndicatorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UCC_WorldIndicatorSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 角色配置没有指定血条控件时使用。 */
	UPROPERTY(Config, EditAnywhere, Category="Health Bar", meta=(MustImplement="/Script/GAS_Demo.CC_WorldIndicatorWidget"))
	TSoftClassPtr<UUserWidget> DefaultHealthBarWidgetClass;

	/** 伤害 / 治疗数字使用的控件。 */
	UPROPERTY(Config, EditAnywhere, Category="Damage Number", meta=(MustImplement="/Script/GAS_Demo.CC_WorldIndicatorWidget"))
	TSoftClassPtr<UUserWidget> DamageNumberWidgetClass;

	/** 每个本地玩家同时显示的伤害数字上限；超出时先移除最旧的普通数字。 */
	UPROPERTY(Config, EditAnywhere, Category="Damage Number", meta=(ClampMin="1"))
	int32 MaxDamageNumbersPerPlayer = 64;

	/** false：只显示自己造成和自己受到的数字；true：显示所有角色的数字。 */
	UPROPERTY(Config, EditAnywhere, Category="Damage Number")
	bool bShowAllDamageNumbers = false;
};
