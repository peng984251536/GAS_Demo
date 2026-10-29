// 生存模式的调参开关（仅开发构建生效；Shipping 下全部返回默认值）。定义在 CC_SurvivalDebug.cpp。
#pragma once

#include "CoreMinimal.h"

namespace CCSurvivalCVars
{
	/** cc.Survival.WaveIntervalScale：波次间隔倍率。 */
	float GetWaveIntervalScale();
	/** cc.Survival.SpawnCountScale：每波数量倍率。 */
	float GetSpawnCountScale();
	/** cc.Survival.XPScale：经验倍率。 */
	float GetExperienceScale();
	/** cc.Survival.Debug：屏幕调试信息。 */
	bool IsDebugDisplayEnabled();
}
