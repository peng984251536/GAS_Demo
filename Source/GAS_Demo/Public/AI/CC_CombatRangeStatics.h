#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CC_CombatRangeStatics.generated.h"

class AActor;

class ACC_BaseCharacter;

/**
 * 战斗距离工具：近战攻击距离按双方碰撞体边缘计算，人群里外圈的人只要贴到前面的人身边就可能够到目标。
 * 行为树装饰器（例如 BTD_Distance_Find）与控制器的被挡检测都用这里的函数，保证判断一致。
 */
UCLASS()
class GAS_DEMO_API UCC_CombatRangeStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 两个 Actor 碰撞体边缘之间的水平距离（角色取胶囊体半径）。
	 * @return 边缘距离，重叠时为 0；任一参数无效时返回一个很大的数。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	static float GetEdgeDistance2D(const AActor* A, const AActor* B);

	/**
	 * @return 角色配置的近战攻击距离（边缘距离）；没有配置时返回默认值 80。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	static float GetMeleeReach(const ACC_BaseCharacter* Character);

	/**
	 * @return Self 与 Target 的边缘距离不超过 Self 的近战攻击距离时返回 true。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	static bool IsInMeleeReach(const ACC_BaseCharacter* SelfActor, const AActor* TargetActor);

	/**
	 * 现在能不能攻击 Target：近战按边缘距离与 MeleeReach（IsInMeleeReach）；
	 * 弓兵按水平中心距离不超过 MaxRange（与 UCC_BowShoot 的射程检查一致）；太近由行为树的后撤分支先处理。
	 * 行为树装饰器「CC In Attack Range」使用它。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	static bool IsInAttackRange(const ACC_BaseCharacter* SelfCharacter, const AActor* Target);

	/**
	 * 是否按弓兵处理：角色配置了箭（ArrowProjectileClass）。与 CC_AttackIntend 的派发规则一致。
	 */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	static bool IsRangedCharacter(const ACC_BaseCharacter* Character);
};
