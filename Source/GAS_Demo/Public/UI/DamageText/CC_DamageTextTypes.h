// 飘字的数据定义：子系统保存命中条目；显示容器按本地 ID 复用 UMG，不为数字创建 Actor 组件。
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CC_DamageTextTypes.generated.h"

/** 飘字样式。决定颜色、字号和是否走强调表现。 */
UENUM(BlueprintType)
enum class ECC_DamageTextStyle : uint8
{
	/** 普通伤害。 */
	Normal,
	/** 暴击：更大字号、强调色，附轻微缩放冲击。 */
	Critical,
	/** 治疗：默认蓝色，带加号，向上飘。 */
	Heal
};

/**
 * 一个正在播放的飘字。
 *
 * 纯数据，不含任何 Slate / UMG 对象；容器读取它来更新数字控件的位置与基础动画。
 * 业务列表只保存数据，显示控件由容器按 ID 复用，不在此结构中持有 UMG。
 */
USTRUCT()
struct FCC_DamageTextEntry
{
	GENERATED_BODY()

	/** 本地稳定 ID；合并时保留，避免列表索引变化导致控件对应错误。 */
	uint64 Id = 0;

	/** 命中时保存的固定世界坐标锚点；每帧重投影适应相机移动，但不自动跟随目标移动。 */
	FVector WorldLocation = FVector::ZeroVector;
    /** 此条目的观看者，分屏时各玩家独立拥有条目。 */
    TWeakObjectPtr<APlayerController> Viewer;
    /** 受击/治疗目标，用于独立排布与合并，避免近距离敌人串字。 */
    TWeakObjectPtr<AActor> Target;
    /** 攻击/治疗来源，不同来源的数字不自动合并。 */
    TWeakObjectPtr<AActor> Source;
    /** 同组伤害类别；当前用于分类，不自动切换元素字体颜色。 */
    FGameplayTag DamageType;

	/** 累计数值。正数=伤害，负数=治疗；合并还会刷新计时、脉冲和样式。 */
	float Amount = 0.0f;

	/** 样式。仅相同样式允许合并，伤害和治疗独立。 */
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
