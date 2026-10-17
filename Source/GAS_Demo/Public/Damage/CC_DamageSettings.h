#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CC_DamageSettings.generated.h"

class UCC_GE_Damage;
class UCC_GE_Healing;

/**
 * 项目设置 → Game → CC Damage：伤害与治疗使用哪个 GE。
 * 默认是原生类；想在编辑器里改 GameplayCue 等配置时，新建继承它们的 GE 资产并在这里指定。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="CC Damage"))
class GAS_DEMO_API UCC_DamageSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UCC_DamageSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 所有伤害使用的 Instant GE；必须继承 CC_GE_Damage。 */
	UPROPERTY(Config, EditAnywhere, Category="Damage")
	TSoftClassPtr<UCC_GE_Damage> DamageEffectClass;
	/** 所有治疗使用的 Instant GE；必须继承 CC_GE_Healing。 */
	UPROPERTY(Config, EditAnywhere, Category="Damage")
	TSoftClassPtr<UCC_GE_Healing> HealingEffectClass;

	/** 配置的伤害 GE 类；未配置或加载失败时返回原生 CC_GE_Damage。 */
	static TSubclassOf<UCC_GE_Damage> GetDamageEffectClass();
	/** 配置的治疗 GE 类；未配置或加载失败时返回原生 CC_GE_Healing。 */
	static TSubclassOf<UCC_GE_Healing> GetHealingEffectClass();

	
};
