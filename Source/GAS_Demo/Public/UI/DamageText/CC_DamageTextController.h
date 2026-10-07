// 伤害飘字的 Controller / Model：飘字子系统 → 控制器 → 展示模型 → CC_DamageTextWidget / SCC_DamageTextLayer。
#pragma once

#include "UI/WorldOverlay/CC_WorldOverlayController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageTextController.generated.h"

class UCC_DamageTextSubsystem;

/**
 * 伤害飘字展示模型：本帧活动飘字的快照与播放时长。
 * 数据逐帧刷新，绘制层在 OnPaint 中直接读取，不广播 OnChanged（见 CC_WorldOverlayController）。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageTextModel : public UCC_UIModel
{
	GENERATED_BODY()
public:
	/** 本帧活动飘字，只读；不要跨帧保存元素引用。 */
	const TArray<FCC_DamageTextEntry>& GetEntries() const { return Entries; }
	/** 飘字总存活时长（秒），绘制层用它计算淡入淡出与上升进度。 */
	float GetLifetime() const { return Lifetime; }
	/** 活动飘字数量，供蓝图调试显示。 */
	UFUNCTION(BlueprintPure, Category="UI|Damage Text")
	int32 GetActiveCount() const { return Entries.Num(); }
private:
	TArray<FCC_DamageTextEntry> Entries;
	float Lifetime = 1.f;
	friend class UCC_DamageTextController;
};

/**
 * 伤害飘字控制器：激活时订阅所在世界的飘字子系统，每次子系统更新后复制活动列表到模型。
 * 命中合并、错位和超时淘汰仍由子系统负责（GameplayCue 通过它 ReportHit）；绘制层不再直接访问子系统。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageTextController : public UCC_WorldOverlayController
{
	GENERATED_BODY()
public:
	/** 指定飘字专用模型。 */
	UCC_DamageTextController();
protected:
	/** 订阅子系统更新、登记绘制层并推送初始快照。 */
	virtual void OnActivated() override;
	/** 解除订阅、撤销绘制层登记并清空快照。 */
	virtual void OnDeactivated() override;
private:
	/** 复制子系统当前的活动飘字与时长。 */
	void Refresh();
	/** 当前订阅的世界子系统；切图后随旧世界失效。 */
	TWeakObjectPtr<UCC_DamageTextSubsystem> BoundSubsystem;
	/** 登记绘制层时使用的玩家，撤销时必须是同一个。 */
	TWeakObjectPtr<APlayerController> RegisteredPlayer;
	/** 本控制器在子系统上的更新订阅句柄。 */
	FDelegateHandle UpdatedHandle;
};
