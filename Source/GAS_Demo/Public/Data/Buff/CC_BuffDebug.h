#pragma once
#include "CoreMinimal.h"

/** Buff 生命周期使用 Log；每次钩子与投射物细节使用 Verbose，避免默认日志刷屏。 */
DECLARE_LOG_CATEGORY_EXTERN(LogCCBuff, Log, All);
