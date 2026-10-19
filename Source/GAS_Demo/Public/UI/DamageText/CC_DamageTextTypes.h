// 伤害数字的样式枚举，玩法侧（GameplayCue）和 UI 侧共用。
#pragma once

#include "CoreMinimal.h"
#include "CC_DamageTextTypes.generated.h"

/** 数字样式：决定颜色、字号以及治疗加号。 */
UENUM(BlueprintType)
enum class ECC_DamageTextStyle : uint8
{
	/** 普通伤害。 */
	Normal,
	/** 暴击：更大字号，出现时有一次缩放冲击。 */
	Critical,
	/** 治疗：带加号。 */
	Heal
};
