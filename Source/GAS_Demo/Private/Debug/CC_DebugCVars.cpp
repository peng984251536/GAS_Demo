// 项目统一调试开关的定义。新增调试开关时在这里加一个 cc.Debug.* 变量，并在 Docs/Debug/DebugCVars.zh-CN.md 登记。
#include "Debug/CC_DebugCVars.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"

namespace
{
	int32 GDebugMessages = 0;
	int32 GDebugMelee = 0;
	int32 GDebugProjectile = 0;

	FAutoConsoleVariableRef CVarDebugMessages(TEXT("cc.Debug.Messages"), GDebugMessages,
		TEXT("C++ 屏幕调试文字（技能激活、寻敌、连招错误等）。0 = 关，1 = 开（单个技能还可用 ShowDebug 关掉）。"));
	FAutoConsoleVariableRef CVarDebugMelee(TEXT("cc.Debug.Melee"), GDebugMelee,
		TEXT("近战判定形状与命中点。0 = 关，1 = 只画勾选 bDrawDebug 的通知，2 = 全部画。"));
	FAutoConsoleVariableRef CVarDebugProjectile(TEXT("cc.Debug.Projectile"), GDebugProjectile,
		TEXT("箭的碰撞球与命中点。0 = 关，1 = 只画勾选 bDrawCollisionDebug 的箭，2 = 全部画。"));

	bool ResolveLevel(int32 Level, bool bAssetFlag)
	{
		return Level >= 2 || (Level == 1 && bAssetFlag);
	}
}

namespace CCDebug
{
	bool ShowMessages() { return GDebugMessages > 0; }
	bool DrawMelee(bool bAssetFlag) { return ResolveLevel(GDebugMelee, bAssetFlag); }
	bool DrawProjectile(bool bAssetFlag) { return ResolveLevel(GDebugProjectile, bAssetFlag); }
}
#endif
