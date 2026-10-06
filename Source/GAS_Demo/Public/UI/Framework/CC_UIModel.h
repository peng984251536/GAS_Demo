#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CC_UIModel.generated.h"

/** 展示快照更新通知；订阅者收到后读取完整状态。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_UIModelChanged);

/** 轻量展示模型：保存 UI 状态，不持有控件，不取代 GAS、存档等真实业务数据。 */
UCLASS(BlueprintType, Blueprintable)
class GAS_DEMO_API UCC_UIModel : public UObject
{
	GENERATED_BODY()
public:
	/** 模型完成一次状态更新后广播；界面按需刷新。 */
	UPROPERTY(BlueprintAssignable, Category="UI|Model")
	FCC_UIModelChanged OnChanged;
protected:
	/** 派生模型写完全部字段后调用一次，避免展示半更新的数据。 */
	UFUNCTION(BlueprintCallable, Category="UI|Model")
	void NotifyChanged() { OnChanged.Broadcast(); }
};
