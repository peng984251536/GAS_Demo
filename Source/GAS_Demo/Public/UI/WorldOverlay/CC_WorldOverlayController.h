// 世界覆盖层控制器基类：头顶血条、伤害飘字等投影到视口的常驻 UI 共用的约定。
#pragma once

#include "UI/Framework/CC_UIController.h"
#include "CC_WorldOverlayController.generated.h"

/**
 * 世界覆盖层控制器基类。
 *
 * 与页面控制器的区别：
 *   - 由绘制层控件（不是 CC_ActivatableWidget）用 CreateForView 创建并持有；
 *     控件 Slate 构建时 Activate，释放时 Release。不进页面栈，没有焦点和关闭语义。
 *   - 数据逐帧变化：子类在数据源每次更新后改写模型，视图在 Paint 中直接读取模型，
 *     模型不广播 OnChanged，避免每帧触发蓝图事件。
 *
 * 每种覆盖层各自一个子类（血条、飘字），数据来源和更新节奏不同，不合并成一个控制器。
 */
UCLASS(Abstract)
class GAS_DEMO_API UCC_WorldOverlayController : public UCC_UIController
{
	GENERATED_BODY()
protected:
	/**
	 * 绘制层登记后调用：同一本地玩家出现第二个同类绘制层时输出警告。
	 * 常见原因是根布局已托管该层，而旧代码/蓝图里又手动添加了一份，结果会画两遍。
	 */
	void WarnIfDuplicateLayer(int32 LayerCount, const TCHAR* LayerName, const TCHAR* Hint) const;
};
