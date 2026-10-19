#pragma once

#include "GameplayEffect.h"
#include "CC_TeamTestTypes.generated.h"

/**
 * 自动化测试使用的魅惑效果：无限持续，授予 CCTags.Status.Charmed。
 * 施加者由测试写入 GE Context 的 Instigator，用来验证有效阵营跟随魅惑者。
 * 不被任何正式玩法资产引用。
 */
UCLASS(NotBlueprintable)
class UCC_TeamTestCharmEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UCC_TeamTestCharmEffect();
};
