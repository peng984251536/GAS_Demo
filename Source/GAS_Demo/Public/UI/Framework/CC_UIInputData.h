#pragma once

#include "CommonInputBaseTypes.h"
#include "CC_UIInputData.generated.h"

/** 官方 CommonUI InputData 工作流的原生默认实现，提供确认与返回动作表。 */
UCLASS()
class GAS_DEMO_API UCC_UIInputData : public UCommonUIInputData
{
	GENERATED_BODY()
public:
	/** 构建默认动作表：Enter/A 确认，Esc/B 返回；可用自己的 InputData 资产替换。 */
	UCC_UIInputData();
};
