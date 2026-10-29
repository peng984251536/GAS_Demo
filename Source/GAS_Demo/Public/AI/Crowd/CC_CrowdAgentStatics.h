#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CC_CrowdAgentStatics.generated.h"

class AAIController;

/**
 * Detour Crowd 避障层的辅助函数。不依赖选目标与登记表；
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

	/**
	 * 按离最近玩家的距离设置避障质量（2 High / 1 Medium / 0 Low），只在等级变化时调用引擎接口。
	 * @param Controller AI 控制器。
	 * @param DistanceToNearestPlayer 离最近玩家 Pawn 的距离（厘米）；没有玩家时传一个很大的数。
	 * @param CurrentLOD 当前等级，-1 表示尚未设置。
	 * @param HighDistance 小于它为 High。
	 * @param MediumDistance 小于它为 Medium，否则 Low。
	 * @return 新的等级；控制器不是 Crowd 控制器时原样返回 CurrentLOD。
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Crowd")
	static int32 UpdateAvoidanceLOD(AAIController* Controller, float DistanceToNearestPlayer, int32 CurrentLOD,
		float HighDistance = 1500.f, float MediumDistance = 4000.f);

	/**
	 * 攻击开始时切到 ObstacleOnly（别人绕开它，它不被推动），结束时恢复 Enabled。
	 * 引擎只允许在路径跟随空闲时切换状态，所以开始攻击时会先停止当前移动。
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Crowd")
	static void SetAttacking(AAIController* Controller, bool bAttacking);
};
