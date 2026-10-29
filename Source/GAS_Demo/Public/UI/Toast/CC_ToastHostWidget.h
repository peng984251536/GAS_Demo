#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "CC_ToastHostWidget.generated.h"

class UPanelWidget;
class UCC_ToastEntryWidget;

/** 宿主里正在显示的一条提示。 */
USTRUCT()
struct FCC_ActiveToast
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UCC_ToastEntryWidget> Widget;

	ECC_ToastPhase Phase = ECC_ToastPhase::Entering;
	/** 当前阶段已过去的秒数。 */
	float PhaseTime = 0.f;
	/** 当前阶段总长：Entering/Leaving 为动画长度，Showing 为显示时长。 */
	float PhaseLength = 0.f;
	/** Showing 阶段的显示时长。 */
	float HoldTime = 0.f;
	FName DedupeKey;
};

/**
 * 提示堆栈：铺满屏幕、不可点击的普通 UserWidget，由 UCC_ToastSubsystem 放进根布局的通知层。
 *
 * 规则：
 *   - 新提示追加在最下面，旧的被往上推；同时最多显示 MaxVisible 条（默认 3）。
 *   - 满了就进队列（优先级高的先出，同优先级先到先出），有条目退场后补上。
 *   - 队列里还有消息时，显示满 MinDisplayTimeWhenQueued 秒的最早一条会提前退场，避免积压。
 *   - 同 DedupeKey 的消息合并到已显示的那条上（刷新内容、重新计时）。
 *   - 计时按真实时间；菜单/弹窗打开时（bHoldWhileMenuOpen）整体隐藏并停止计时。
 *
 * 编辑器里制作（可选）：新建 WBP 继承本类，放一个名为 Panel_Stack 的 VerticalBox 决定位置；
 * 条目类在类默认值 Entry Class 里选，或在项目设置 CC Toast → Entry Widget Class 里统一指定。
 * 设计器为空时使用原生布局（屏幕下方居中）。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_ToastHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UCC_ToastHostWidget(const FObjectInitializer& ObjectInitializer);

	void Enqueue(const FCC_ToastRequest& Request);
	/** 推进计时；DeltaTime 为真实时间。 */
	void Advance(float DeltaTime);
	/** 立即清空全部提示（不播退场动画）。 */
	UFUNCTION(BlueprintCallable, Category="Toast")
	void ClearAll();
	bool HasWork() const { return !Active.IsEmpty() || !Queue.IsEmpty(); }

	/** 子系统按项目设置写入。 */
	void SetHoldWhileMenuOpen(bool bHold) { bHoldWhileMenuOpen = bHold; }
	/** 项目设置里指定了条目类时覆盖类默认值。 */
	void SetEntryClassOverride(TSubclassOf<UCC_ToastEntryWidget> InClass);

protected:
	virtual void NativeOnInitialized() override;
	virtual void BuildNativeLayout();

	UPROPERTY(EditDefaultsOnly, Category="Toast")
	TSubclassOf<UCC_ToastEntryWidget> EntryClass;

	/** 同时显示的最大条数。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast", meta=(ClampMin="1"))
	int32 MaxVisible = 3;
	/** 最多排队条数，超出丢弃最低优先级里最早的。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast", meta=(ClampMin="1"))
	int32 MaxQueued = 16;
	/** Request.Duration <= 0 时的显示秒数。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast", meta=(ClampMin="0.1"))
	float DefaultDuration = 3.f;
	/** 有消息排队时，最早一条显示满这么久就提前退场让位；0 表示不提前，严格等它播完。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast", meta=(ClampMin="0"))
	float MinDisplayTimeWhenQueued = 1.f;
	/** 条目之间的间距（像素）。 */
	UPROPERTY(EditDefaultsOnly, Category="Toast", meta=(ClampMin="0"))
	float EntrySpacing = 8.f;

	UPROPERTY(BlueprintReadOnly, Category="Toast", meta=(BindWidgetOptional))
	TObjectPtr<UPanelWidget> Panel_Stack;

private:
	FCC_ActiveToast* FindActive(FName DedupeKey);
	void StartToast(const FCC_ToastRequest& Request);
	void BeginLeaving(FCC_ActiveToast& Toast);
	int32 CountActive(bool bIncludeLeaving) const;
	bool IsHeldByMenu() const;

	UPROPERTY(Transient)
	TArray<FCC_ActiveToast> Active;
	FCC_ToastQueue Queue;
	bool bHoldWhileMenuOpen = true;
	bool bHeld = false;
};
