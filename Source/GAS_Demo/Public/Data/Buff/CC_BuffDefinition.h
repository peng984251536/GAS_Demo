#pragma once

#include "CoreMinimal.h"
#include "Effects/CC_GameplayEffectBase.h"
#include "CC_BuffDefinition.generated.h"

class UTexture2D;
class UCombatActionSet;
class UCC_BuffBehavior;
class UGameplayEffectComponent;
class APawn;

/**
 * 角色可获得的 buff 共享配置，编辑器中使用数据蓝图配置，运行时按只读数据使用。
 * 修饰器、执行计算、标签条件、触发组件、持续时间与堆叠规则直接继承 UE 的 GameplayEffect。
 * 配置蓝图放在 Data/Buff 下；本类只补充显示信息与项目特殊玩法，不保存角色实例状态。
 */
UCLASS(BlueprintType, Blueprintable)
class GAS_DEMO_API UCC_BuffDefinition : public UCC_GameplayEffectBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display")
	FText DisplayName;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display", meta=(MultiLine=true))
	FText Description;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Display")
	TObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Lifecycle")
	bool bRemoveOnDeath = true;
	/** Optional game-specific action override; native GE components still handle native application requirements. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Special")
	TObjectPtr<UCombatActionSet> AttackActionSetOverride;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Special")
	TSubclassOf<APawn> SummonClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Special", meta=(ClampMin="1", ClampMax="32"))
	int32 SummonCount = 1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Special", meta=(ClampMin="0"))
	float SummonRadius = 150.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buff|Special")
	bool bDestroySummonsOnRemoval = true;
	/** One duplicate per active GE handle; native stack increases do not repeat OnApplied. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category="Buff|Special")
	TArray<TObjectPtr<UCC_BuffBehavior>> Behaviors;

	/**
	 * 读取原生 GE 组件模板，供 buffEffect 初始化时创建自己的组件副本。
	 * 这些对象属于共享配置，运行时不能用来保存角色实例状态。
	 */
	const TArray<TObjectPtr<UGameplayEffectComponent>>& GetConfiguredComponents() const { return GEComponents; }
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
