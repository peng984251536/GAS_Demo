// 世界级数据管理器：维护活动飘字列表，处理近距离命中合并与超时淘汰；绘制由 HUD 层完成。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageTextSubsystem.generated.h"

/**
 * 伤害飘字的数据与生命周期管理。
 *
 * 定位：只持有"还没播完的飘字"这一份活动列表，不做任何绘制。
 * 绘制在 SCC_DamageTextLayer 里一次性完成——这是性能上的关键分工。
 *
 * 为什么不用"每个数字一个 WidgetComponent"：
 *   每个 WidgetComponent 会创建独立的 Slate 控件并渲染到一张 DrawToRenderTarget 纹理上。
 *   多个世界空间 WidgetComponent 会增加独立渲染目标、控件布局与绘制开销；实际 draw call 数需运行时测量。
 *   本方案所有飘字共用一个 Slate 控件，在 OnPaint 里批量绘制，开销与数量近似线性。
 *
 * 联机约定：伤害数值由 GameplayCue 复制到各客户端，因此本子系统只在客户端持有数据。
 * 专用服务器不保存飘字；监听服务器还需要为本机玩家显示表现。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageTextSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- USubsystem ---
	/** 专用服务器不创建；单机、客户端和监听服务器均可创建。 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// --- FTickableGameObject ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	/** 没有活动飘字时自动停止 Tick，空闲期零开销。 */
	virtual bool IsTickable() const override;

	/**
	 * 提交一次命中表现。由 GameplayCue 在客户端调用。
	 *
	 * @param WorldLocation    飘字起点；通常取受击者骨骼位置，不是 Actor 原点。
	 * @param Amount           已确定的表现数值。正数=伤害，负数=治疗。
	 * @param Style            样式，决定颜色和字号；默认普通伤害。
	 */
	void ReportHit(const FVector& WorldLocation, float Amount,
		ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal);

	/** 当前活动飘字，仅供绘制层只读遍历。 */
	const TArray<FCC_DamageTextEntry>& GetActiveEntries() const { return ActiveEntries; }

	/** 合并窗口（秒）。距上次命中不超过该时间且锚点接近的条目可以累加；当前不按 Actor 身份区分。 */
	UPROPERTY(EditAnywhere, Category = "Damage Text", meta = (ClampMin = "0.0"))
	float MergeWindow = 0.25f;

	/**
	 * 合并距离阈值（厘米，世界空间）。
	 * 只有世界锚点足够近才合并；距离判定不能保证目标相同，靠近的不同敌人仍可能合并。
	 * 默认 120 厘米；密集敌人场景可缩小阈值，或将 MergeWindow 设为 0 禁用合并。
	 */
	UPROPERTY(EditAnywhere, Category = "Damage Text", meta = (ClampMin = "0.0"))
	float MergeDistance = 120.0f;

	/** 飘字总存活时长（秒），从该条目最后一次累加算起。 */
	UPROPERTY(EditAnywhere, Category = "Damage Text", meta = (ClampMin = "0.1"))
	float Lifetime = 1.1f;

	/**
	 * 最近一次 ReportHit 是否发生了合并（1=合并进了已有飘字，0=新建了一个飘字）。
	 * 供调试和性能观测；单次命中最多合并进一个飘字，所以这里不会大于 1。
	 * 不需要可以删掉。
	 */
	int32 LastMergeCount = 0;

private:
	/** 活动列表。新建时插入最前；超时删除采用交换移除，因此不保证长期保持新旧顺序。 */
	TArray<FCC_DamageTextEntry> ActiveEntries;

	/** 尝试把本次命中并入已有飘字；成功则刷新计时并返回 true。 */
	bool TryMerge(const FVector& WorldLocation, float Amount, ECC_DamageTextStyle Style);

	/** 老化淘汰：移除已经播完的条目。 */
	void AgeEntries(float DeltaTime);

	/** 建议的起始屏幕偏移，按当前活动数量错开，避免密集命中时完全重叠。 */
	FVector2D NextStackOffset() const;
};
