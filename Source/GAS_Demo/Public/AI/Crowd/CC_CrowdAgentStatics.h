#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AI/Engagement/CC_EngagementTypes.h"
#include "CC_CrowdAgentStatics.generated.h"

class AAIController;

/**
 * Detour Crowd 避障层的辅助函数。只认档位枚举，不知道槽位和令牌的存在；
 * 控制器的 PathFollowingComponent 不是 UCrowdFollowingComponent 时全部静默跳过。
 */
UCLASS()
class GAS_DEMO_API UCC_CrowdAgentStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 附身后的默认避障参数：开启分离与转弯预判，初始质量 Low。 */
	UFUNCTION(BlueprintCallable, Category = "AI|Crowd")
	static void ConfigureCrowdAgent(AAIController* Controller);

	/** 按档位设置避障质量：None / Queue → Low，Surround → Medium，Engaged → High。 */
	UFUNCTION(BlueprintCallable, Category = "AI|Crowd")
	static void ApplyTier(AAIController* Controller, ECC_EngagementTier Tier);

	/**
	 * 攻击开始时切到 ObstacleOnly（别人绕开它，它不被推动），结束时恢复 Enabled。
	 * 引擎只允许在路径跟随空闲时切换状态，所以开始攻击时会先停止当前移动。
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Crowd")
	static void SetAttacking(AAIController* Controller, bool bAttacking);
};
