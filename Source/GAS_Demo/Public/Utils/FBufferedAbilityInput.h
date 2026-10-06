#pragma once

#include "GameplayTagContainer.h"

/**
 * 输入与当前能力的衔接方式。
 */
UENUM(BlueprintType)
enum class ECCAbilityLinkType : uint8
{
	None,

	// 当前攻击能力内部进入下一段
	Combo,

	// 取消当前能力，立即执行新能力
	Cancel,

	// 当前能力正常结束后执行新能力
	Queue
};

/**
 * 暂存一次尚未消费的能力输入。
 *
 * 当前版本只保存一个输入：
 * 后输入的会覆盖先输入的。
 */
struct FBufferedAbilityInput
{
	// 玩家希望执行的能力
	FGameplayTag AbilityTag;

	// Queue 模式下，需要等待哪个能力结束
	FGameplayTag WaitForAbilityTag;

	// 输入发生的本地世界时间
	float InputTime = 0.0f;

	// 这次输入的衔接方式
	ECCAbilityLinkType LinkType = ECCAbilityLinkType::None;

	bool IsValid() const
	{
		return AbilityTag.IsValid()
			&& LinkType != ECCAbilityLinkType::None;
	}

	void Reset()
	{
		AbilityTag = FGameplayTag();
		WaitForAbilityTag = FGameplayTag();
		InputTime = 0.0f;
		LinkType = ECCAbilityLinkType::None;
	}
};