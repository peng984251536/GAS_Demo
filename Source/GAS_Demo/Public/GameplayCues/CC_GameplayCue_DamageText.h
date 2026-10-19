#pragma once

#include "GameplayCueNotify_Static.h"
#include "NativeGameplayTags.h"
#include "CC_GameplayCue_DamageText.generated.h"

namespace CCDamageCueTags
{
	/** 手动显示工具（UCC_DamageTextStatics）没有来源时挂上，跳过"只看自己相关伤害"的筛选。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ManualDisplay);
}

/**
 * 伤害数字的客户端表现，由伤害 GE 上配置的 GameplayCue.Combat.DamageText 触发。
 * 只做显示：决定数字位置（有命中点用命中点，否则用目标头顶）、样式、给哪些本地玩家看，然后交给 UCC_DamageNumberSource。
 */
UCLASS()
class GAS_DEMO_API UCC_GameplayCue_DamageText : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
public:
	UCC_GameplayCue_DamageText();
	virtual bool HandlesEvent(EGameplayCueEvent::Type Type) const override { return Type == EGameplayCueEvent::Executed; }
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Params) const override;

protected:
	/** 没有命中点时，数字出现在目标包围盒顶部上方多少厘米。 */
	UPROPERTY(EditDefaultsOnly, Category="Damage Text") float HeadOffset = 20.f;
	/** true 表示这是治疗数字。 */
	bool bHealing = false;
};

/** 治疗数字，由治疗 GE 上的 GameplayCue.Combat.HealText 触发。 */
UCLASS()
class GAS_DEMO_API UCC_GameplayCue_HealText : public UCC_GameplayCue_DamageText
{
	GENERATED_BODY()
public:
	UCC_GameplayCue_HealText();
};
