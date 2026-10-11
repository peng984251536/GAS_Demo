// 世界覆盖层控制器基类：头顶血条等需要 MVC 协调的常驻 UI 共用约定；伤害飘字已改为直接读取子系统。
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
 * 复杂覆盖层按功能使用各自子类；简单飘字不再创建控制器。
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
