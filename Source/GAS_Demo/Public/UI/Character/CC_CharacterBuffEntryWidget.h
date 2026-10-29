#pragma once

#include "Blueprint/UserWidget.h"
#include "UI/Character/CC_CharacterScreenTypes.h"
#include "CC_CharacterBuffEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;

/**
 * 角色面板里的一条 Buff。子控件全部可选，WBP 按名称放置即可：
 * Icon / NameText / DescriptionText / TimeText / TimeBar。
 * 剩余时间每帧按服务器时钟计算（与 CC_BuffBarWidget 一致），不经过模型。
 */
UCLASS(Abstract, Blueprintable)
class GAS_DEMO_API UCC_CharacterBuffEntryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 写入数据并刷新静态部分（图标、名称、描述），随后触发 On Buff Set。 */
	UFUNCTION(BlueprintCallable, Category="UI|Buff")
	void SetBuff(const FCC_CharacterBuffEntry& InBuff);

	UFUNCTION(BlueprintPure, Category="UI|Buff")
	const FCC_CharacterBuffEntry& GetBuff() const { return Buff; }

	/** 剩余秒数；无限期返回 -1。 */
	UFUNCTION(BlueprintPure, Category="UI|Buff")
	float GetRemainingTime() const;

protected:
	/** 更新倒计时文字和进度条。 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 蓝图扩展：按 bIsDebuff 切换配色、按 bIsInhibited 半透明等。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Buff")
	void OnBuffSet(const FCC_CharacterBuffEntry& InBuff);

	/** 层数大于 1 时名称的格式。 */
	UPROPERTY(EditAnywhere, Category="UI|Buff")
	FText NameWithStacksFormat = NSLOCTEXT("CCCharacterScreen", "NameWithStacks", "{Name} ×{Stacks}");

	/** 无限期效果在 TimeText 中显示的文字。 */
	UPROPERTY(EditAnywhere, Category="UI|Buff")
	FText InfiniteDurationText = NSLOCTEXT("CCCharacterScreen", "InfiniteDuration", "持续");

	UPROPERTY(BlueprintReadOnly, Category="UI|Buff", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Icon;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TimeText;
	/** 剩余时间 / 总时长；无限期时为满。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff", meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> TimeBar;

private:
	void UpdateTime();

	UPROPERTY(Transient) FCC_CharacterBuffEntry Buff;
};
