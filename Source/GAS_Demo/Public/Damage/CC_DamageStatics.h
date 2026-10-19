#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Damage/CC_DamageTypes.h"
#include "GameplayEffectTypes.h"
#include "CC_DamageStatics.generated.h"

class UAbilitySystemComponent;

/**
 * 伤害与治疗的唯一入口（CLAUDE.md 规则 3），只在服务器调用。
 * 这里只校验输入并构造 Spec；计算、扣血、飘字、伤害事件都由 GE 和 GAS 完成，调用方不用再做任何事。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** 立即对目标造成一次伤害（调用时快照攻击方属性）。 */
	UFUNCTION(BlueprintCallable, Category="GAS|Damage")
	static ECC_DamageApplyStatus ApplyDamage(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
		const FCC_DamageRequest& Request);

	/**
	 * 投射物发射时准备伤害快照：此刻捕获攻击方属性，命中时再用 ApplyPreparedDamage 应用。
	 * @param SourceObject 本次伤害的来源对象，例如箭。
	 * @return 参数无效或非权威端时返回空句柄。
	 */
	static FGameplayEffectSpecHandle PrepareDamage(UAbilitySystemComponent* SourceASC, const FCC_DamageRequest& Request,
		UObject* SourceObject = nullptr);

	/** 应用 PrepareDamage 得到的快照；每次命中复制一份 Spec 和 Context，多个目标互不影响。 */
	static ECC_DamageApplyStatus ApplyPreparedDamage(UAbilitySystemComponent* TargetASC,
		const FGameplayEffectSpecHandle& Snapshot, const FHitResult* Hit = nullptr);

	/** 治疗目标；超过最大生命的部分不生效。 */
	UFUNCTION(BlueprintCallable, Category="GAS|Damage")
	static ECC_DamageApplyStatus ApplyHealing(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC,
		float Amount);
};
