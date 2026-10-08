// 头顶血条的 Controller / Model：血条子系统 → 控制器 → 展示模型 → CC_BatchedHealthBarWidget（摆放单条血条控件）。
#pragma once

#include "UI/WorldOverlay/CC_WorldOverlayController.h"
#include "UI/Framework/CC_UIModel.h"
#include "UI/HealthBar/CC_HealthBarTypes.h"
#include "CC_HealthBarOverlayController.generated.h"

class UCC_BatchedHealthBarSubsystem;

/** 一条需要显示的血条：已按显隐、就绪、死亡/满血规则过滤，只剩投影和距离剔除留给视图。 */
struct FCC_HealthBarDisplayItem
{
	/** 血条所属角色；视图按它复用单条控件，并在分配时传给控件。弱引用，不延长角色生命。 */
	TWeakObjectPtr<AActor> Actor;
	/** 单条控件类；为空时视图使用默认控件。 */
	TSubclassOf<UUserWidget> ItemWidgetClass;
	/** 世界锚点 = 角色位置 + Options.WorldOffset，单位厘米。 */
	FVector WorldAnchor = FVector::ZeroVector;
	/** 最远显示距离，0 表示不剔除；需要相机位置，所以由视图判断。 */
	float MaxDistance = 0.f;
	/** 推送给单条控件的数据。 */
	FCC_HealthBarItemData Data;
};

/**
 * 头顶血条展示模型：保存本帧要显示的血条快照，不持有 ASC 或控件，角色只以弱引用作为身份。
 * 数据逐帧刷新，视图在 NativeTick 中直接读取，不广播 OnChanged（见 CC_WorldOverlayController）。
 */
UCLASS()
class GAS_DEMO_API UCC_HealthBarOverlayModel : public UCC_UIModel
{
	GENERATED_BODY()
public:
	/** 本帧待绘制的血条，只读；不要跨帧保存元素引用。 */
	const TArray<FCC_HealthBarDisplayItem>& GetItems() const { return Items; }
	/** 待绘制数量，供蓝图调试显示。 */
	UFUNCTION(BlueprintPure, Category="UI|Health Bar")
	int32 GetVisibleCount() const { return Items.Num(); }
private:
	TArray<FCC_HealthBarDisplayItem> Items;
	friend class UCC_HealthBarOverlayController;
};

/**
 * 头顶血条控制器：激活时订阅本地玩家的血条子系统，每次子系统更新后把条目转换为展示快照。
 * 注册、GAS 订阅与血量插值仍由子系统负责（敌人和蓝图通过它注册）；视图不再直接访问子系统。
 */
UCLASS()
class GAS_DEMO_API UCC_HealthBarOverlayController : public UCC_WorldOverlayController
{
	GENERATED_BODY()
public:
	/** 指定血条专用模型。 */
	UCC_HealthBarOverlayController();
protected:
	/** 订阅子系统更新、登记绘制层并推送初始快照。 */
	virtual void OnActivated() override;
	/** 解除订阅、撤销绘制层登记并清空快照。 */
	virtual void OnDeactivated() override;
private:
	/** 按显示规则过滤子系统条目，写入模型。 */
	void Refresh();
	/** 当前订阅的子系统；弱引用，不延长 LocalPlayer 子系统生命。 */
	TWeakObjectPtr<UCC_BatchedHealthBarSubsystem> BoundManager;
	/** 本控制器在子系统上的更新订阅句柄。 */
	FDelegateHandle UpdatedHandle;
};
