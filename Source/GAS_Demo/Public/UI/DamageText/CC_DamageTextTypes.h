// 飘字的数据定义：每次命中只生成结构体条目，不生成 Widget 或 Actor 组件。
#pragma once

#include "CoreMinimal.h"
#include "CC_DamageTextTypes.generated.h"

/** 飘字样式。决定颜色、字号和是否走强调表现。 */
UENUM(BlueprintType)
enum class ECC_DamageTextStyle : uint8
{
	/** 普通伤害。 */
	Normal,
	/** 暴击：更大字号、强调色，附轻微缩放冲击。 */
	Critical,
	/** 治疗：绿色，向上飘。 */
	Heal
};

/**
 * 一个正在播放的飘字。
 *
 * 纯数据，不含任何 Slate / UMG 对象——绘制层按帧读取这份数据并批量画。
 * 这样飘字的数量不会线性增加控件数量，是本方案性能优势的来源。
 */
USTRUCT()
struct FCC_DamageTextEntry
{
	GENERATED_BODY()

	/** 命中时保存的固定世界坐标锚点；每帧重投影适应相机移动，但不自动跟随目标移动。 */
	FVector WorldLocation = FVector::ZeroVector;

	/** 累计数值。正数=伤害，负数=治疗；合并还会刷新计时、脉冲和样式。 */
	float Amount = 0.0f;

	/** 样式。合并时以较高优先级覆盖（暴击 > 治疗 > 普通）。 */
	ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal;

	/** 距最近一次新建或合并命中的时间（秒）；合并后重新从 0 开始。 */
	float Elapsed = 0.0f;

	/** 起始屏幕偏移，用来错开同一位置的多个飘字。 */
	FVector2D StackOffset = FVector2D::ZeroVector;

	/**
	 * 合并时额外增加的"跳动"强度，0..1。
	 * 连击累加时给一点二次弹跳，让玩家能感知到命中还在继续。
	 */
	float Pulse = 0.0f;
};
