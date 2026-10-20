#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BowRelease.generated.h"

/**
 * 放在弓箭攻击蒙太奇的「弓弦松开」帧。
 * 只负责隐藏手上用于演出的箭模型，并通知当前角色发生了放箭事件。
 * 投射物由正在执行的射击能力生成；AnimNotify 不保存每个角色的运行时状态。
 */
UCLASS(meta=(DisplayName="Bow Release"))
class GAS_DEMO_API UAnimNotify_BowRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_BowRelease();

	/**
	 * 要隐藏的 StaticMeshComponent 名称或 Component Tag。
	 * 现有弓兵蓝图中的手持箭模型名为 Arrow02SM；不要填 UE 自带的 ArrowComponent 指示器。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bow Release")
	FName HeldArrowComponentName = TEXT("Arrow02SM");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bow Release")
	bool SetVisibility = false;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	
	virtual FString GetNotifyName_Implementation() const override;
};
