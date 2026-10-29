#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "CC_ToastEntryWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UWidgetAnimation;

/**
 * 一条提示。由 UCC_ToastHostWidget 创建、计时和回收，自己不计时、不 RemoveFromParent。
 *
 * 编辑器里制作（新建 WBP 继承本类）：
 *   控件（BindWidgetOptional，按名字自动绑定，缺了不报错）：
 *     TextBlock_Title、TextBlock_Message（TextBlock）、Image_Icon（Image）、Border_Accent（Border，设为强调色）。
 *   动画（BindWidgetAnimOptional，在 Animations 面板按名字新建）：
 *     Anim_In      进场：从下往上 + 淡入，例如 RenderTransform.Translation.Y 40→0、RenderOpacity 0→1。
 *     Anim_Out     退场：继续往上 + 淡出，例如 Translation.Y 0→-30、RenderOpacity 1→0。结束帧必须是完全透明。
 *     Anim_Refresh 可选：同 DedupeKey 合并刷新时播一次（例如轻微放大回弹）。
 *   动画时长就是进出场时长，系统按动画长度计时，不需要另外配置。
 * 设计器为空（纯 C++ 类）时使用原生示例布局；WBP 没做 Anim_In / Anim_Out 时用代码做同样的上浮淡入淡出兜底。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_ToastEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCC_ToastEntryWidget(const FObjectInitializer& ObjectInitializer);

	/** 写入内容（首次显示或合并刷新）：刷新文字、图标、强调色，触发 On Toast Applied。 */
	void ApplyRequest(const FCC_ToastRequest& Request);

	/** 开始进场；返回进场秒数。 */
	float PlayIntro();
	/** 开始退场（会打断未播完的进场）；返回退场秒数。 */
	float PlayOutro();
	/** 宿主每帧调用；只在没有对应 UMG 动画时生效（代码兜底的上浮淡入淡出）。Alpha 为阶段进度 0→1。 */
	void UpdateFallback(ECC_ToastPhase Phase, float Alpha);

	UFUNCTION(BlueprintPure, Category="Toast")
	const FCC_ToastRequest& GetRequest() const { return CurrentRequest; }

	/** 风格对应的强调色（类默认值 Style Colors）。 */
	UFUNCTION(BlueprintPure, Category="Toast")
	FLinearColor GetStyleColor(ECC_ToastStyle Style) const;

protected:
	virtual void NativeOnInitialized() override;
	/** 设计器为空时构建原生示例布局。 */
	virtual void BuildNativeLayout();

	/** 内容写入后。bMerged=true 表示被同 DedupeKey 的新消息刷新。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Toast", meta=(DisplayName="On Toast Applied"))
	void BP_OnToastApplied(const FCC_ToastRequest& Request, FLinearColor AccentColor, bool bMerged);

	/** 进入 Entering / Showing / Leaving 时各触发一次。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Toast", meta=(DisplayName="On Toast Phase Changed"))
	void BP_OnToastPhaseChanged(ECC_ToastPhase NewPhase);

	UPROPERTY(Transient, BlueprintReadOnly, Category="Toast|Animation", meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_In;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Toast|Animation", meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Out;
	UPROPERTY(Transient, BlueprintReadOnly, Category="Toast|Animation", meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> Anim_Refresh;

	/** 没有 Anim_In 时的代码进场秒数。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast|Fallback Animation", meta=(ClampMin="0"))
	float FallbackIntroTime = 0.25f;
	/** 没有 Anim_Out 时的代码退场秒数。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast|Fallback Animation", meta=(ClampMin="0"))
	float FallbackOutroTime = 0.3f;
	/** 代码兜底：进场从下方多少像素升上来，退场再往上移动多少像素。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast|Fallback Animation")
	float FallbackRiseDistance = 32.f;

	UPROPERTY(EditDefaultsOnly, Category="Toast|Style")
	TMap<ECC_ToastStyle, FLinearColor> StyleColors;

	UPROPERTY(BlueprintReadOnly, Category="Toast", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TextBlock_Title;
	UPROPERTY(BlueprintReadOnly, Category="Toast", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TextBlock_Message;
	UPROPERTY(BlueprintReadOnly, Category="Toast", meta=(BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;
	UPROPERTY(BlueprintReadOnly, Category="Toast", meta=(BindWidgetOptional))
	TObjectPtr<UBorder> Border_Accent;

private:
	void NotifyPhase(ECC_ToastPhase Phase);
	static float GetAnimLength(const UWidgetAnimation* Animation);

	FCC_ToastRequest CurrentRequest;
	bool bHasContent = false;
	bool bPhaseNotified = false;
	ECC_ToastPhase LastPhase = ECC_ToastPhase::Entering;
};
