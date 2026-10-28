#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NativeGameplayTags.h"
#include "GameplayTagContainer.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "CC_ToastSettings.generated.h"

class UCC_ToastHostWidget;
class UCC_ToastEntryWidget;

/** 提示系统自用的原生标签（不属于对局消息的提示来源）。 */
namespace CCToastTags
{
	/** 下一波即将开始的预告（客户端根据 NextWaveServerTime 自己算，不走网络消息）。 */
	GAS_DEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(WaveIncoming);
}

/**
 * 项目设置 → Game → CC Toast。
 * 改外观：做一个继承 CC_ToastEntryWidget 的 WBP（含 Anim_In / Anim_Out 动画）填到 Entry Widget Class；
 *         要改堆栈位置/条数，再做一个继承 CC_ToastHostWidget 的 WBP 填到 Host Widget Class。
 * 改文案：在 Message Toasts 里按标签加一行（Survival.Msg.* 或 UI.Toast.WaveIncoming），覆盖内置默认；
 *         把 bEnabled 关掉即可屏蔽某类提示。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="CC Toast"))
class GAS_DEMO_API UCC_ToastSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** 单条提示控件（在编辑器里做动画的那个 WBP）；留空使用宿主类默认值里的 Entry Class。 */
	UPROPERTY(Config, EditAnywhere, Category="Toast")
	TSoftClassPtr<UCC_ToastEntryWidget> EntryWidgetClass;

	/** 提示堆栈控件；留空使用原生 CC_ToastHostWidget（屏幕下方居中，最多 3 条）。 */
	UPROPERTY(Config, EditAnywhere, Category="Toast")
	TSoftClassPtr<UCC_ToastHostWidget> HostWidgetClass;

	/** 暂停、选 Buff 等菜单/弹窗打开时，提示隐藏并停止计时，关闭后继续。 */
	UPROPERTY(Config, EditAnywhere, Category="Toast")
	bool bHoldWhileMenuOpen = true;

	/** 根布局尚未就绪（刚进图）时最多先缓存几条。 */
	UPROPERTY(Config, EditAnywhere, Category="Toast", meta=(ClampMin="0"))
	int32 MaxPendingBeforeLayout = 8;

	/** 对局消息 → 提示模板。没有配置的标签使用内置默认（见 UCC_SurvivalToastRelay::GetBuiltinTemplate）。 */
	UPROPERTY(Config, EditAnywhere, Category="Survival", meta=(Categories="Survival.Msg,UI.Toast"))
	TMap<FGameplayTag, FCC_ToastTemplate> MessageToasts;

	/** 是否为没有配置的对局消息使用内置默认提示。关掉后只显示 MessageToasts 里配置的。 */
	UPROPERTY(Config, EditAnywhere, Category="Survival")
	bool bUseBuiltinMessageToasts = true;

	/** 下一波开始前多少秒弹"敌人即将来袭"预告；0 关闭。 */
	UPROPERTY(Config, EditAnywhere, Category="Survival", meta=(ClampMin="0"))
	float WaveIncomingLeadSeconds = 5.f;
};
