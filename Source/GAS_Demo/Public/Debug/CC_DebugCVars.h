// 项目统一的调试开关（控制台变量 cc.Debug.*）。说明见 Docs/Debug/DebugCVars.zh-CN.md。
// 所有开关默认关闭：Development 测试包启动后不画线、不打屏幕字；需要时按 ~ 输入命令或写进 Saved/Config/Windows/Engine.ini。
// Shipping 下变量不存在，查询函数恒为 false，调试代码被编译器整段剔除。
#pragma once

#include "CoreMinimal.h"

namespace CCDebug
{
#if UE_BUILD_SHIPPING
	inline bool ShowMessages() { return false; }
	inline bool DrawMelee(bool /*bAssetFlag*/) { return false; }
	inline bool DrawProjectile(bool /*bAssetFlag*/) { return false; }
#else
	/** cc.Debug.Messages：C++ 屏幕调试文字（技能激活、寻敌、连招错误等）。0 = 关，1 = 开。 */
	GAS_DEMO_API bool ShowMessages();

	/**
	 * cc.Debug.Melee：近战判定形状与命中点。
	 * 0 = 关；1 = 只画勾选了 bDrawDebug 的动画通知；2 = 全部画。
	 */
	GAS_DEMO_API bool DrawMelee(bool bAssetFlag);

	/**
	 * cc.Debug.Projectile：箭的碰撞球与命中点。
	 * 0 = 关；1 = 只画勾选了 bDrawCollisionDebug 的箭；2 = 全部画。
	 */
	GAS_DEMO_API bool DrawProjectile(bool bAssetFlag);
#endif
}
