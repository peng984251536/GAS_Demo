#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CC_TeamStatics.generated.h"

class AActor;

/**
 * 全项目唯一的敌我判断入口。AI 选目标、近战判定、投射物命中、范围伤害都应调用 IsHostile，
 * 不要再用 Actor Tag 或直接比较阵营标签判断敌我。
 *
 * 阵营分两层：
 *   基础阵营：实例覆盖（ACC_BaseCharacter::SetTeamOverride，召唤物继承召唤者）优先，
 *             其次 CharacterConfig.Team，为空时用角色类默认值（ACC_BaseCharacter::GetDefaultTeam）。
 *   有效阵营：被魅惑（CCTags.Status.Charmed）时临时变为施加魅惑者的基础阵营。
 * 非角色 Actor（如箭）使用其 Instigator 的阵营，因此反弹投射物只需更换 Instigator。
 *
 * 魅惑判断需要读取活动 GE，只在服务器上可靠（敌人 ASC 使用 Minimal 复制模式）。
 */
UCLASS()
class GAS_DEMO_API UCC_TeamStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 判断 A 是否把 B 视为敌人。
	 * @param A 发起判断的一方，例如 AI 自己、攻击者或投射物。
	 * @param B 被判断的一方。
	 * @return 双方有效阵营都存在且不同则返回 true；同一个 Actor、任一方为空或没有阵营返回 false。
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	static bool IsHostile(const AActor* A, const AActor* B);

	/**
	 * 获取 Actor 当前生效的阵营，已考虑魅惑等临时状态。
	 * @param Actor 角色或由角色发起的 Actor（投射物、召唤物等）。
	 * @return 被魅惑时为施加最新一次魅惑者的基础阵营，否则为基础阵营；无法确定时返回空标签。
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	static FGameplayTag GetEffectiveTeam(const AActor* Actor);

	/**
	 * 获取 Actor 的基础阵营，不考虑魅惑等临时状态。
	 * @param Actor 角色或由角色发起的 Actor；非角色时沿 Instigator 查找一次。
	 * @return 依次取实例覆盖、CharacterConfig.Team、角色类默认阵营；都没有时返回空标签。
	 */
	UFUNCTION(BlueprintPure, Category = "Team")
	static FGameplayTag GetBaseTeam(const AActor* Actor);
};
