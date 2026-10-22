#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "CC_SummonAlly.generated.h"

class ACC_EnemyCharacter;

/**
 * 召唤随从（由召唤 Buff 授予，授予即激活，只在服务器执行）。
 * 在施放者身边生成一个 SummonClass，阵营改为施放者的阵营，并通过 UCC_SummonBuffShareComponent
 * 同步施放者的 Buff；然后移除授予本能力的 Buff。召唤物不能再召唤。
 */
UCLASS()
class GAS_DEMO_API UCC_SummonAlly : public UCC_GameplayAbilityBase
{
	GENERATED_BODY()

public:
	UCC_SummonAlly();

protected:
	/** 召唤的敌人蓝图；生成后阵营改为施放者的阵营。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon")
	TSubclassOf<ACC_EnemyCharacter> SummonClass;

	/** 召唤点离施放者的水平距离（厘米）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Summon", meta = (ClampMin = "50"))
	float SpawnDistance = 150.f;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** 从施放者身后开始，按 45° 依次尝试 8 个方向，返回第一个生成成功的随从。 */
	ACC_EnemyCharacter* SpawnNearCaster() const;
};
