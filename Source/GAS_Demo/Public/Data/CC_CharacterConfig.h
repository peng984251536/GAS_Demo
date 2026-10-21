#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CC_CharacterConfig.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UCombatActionSet;
class UCombatActionData;
class ACC_ArrowProjectile;
class UUserWidget;

/** 头顶血条的配置。只是数据，玩法代码不读取；由 UI 侧的 UCC_HealthBarSource 读取。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_OverheadHealthBarConfig
{
	GENERATED_BODY()
	/** 血条控件（任意实现 CC World Indicator Widget 的 UserWidget，推荐派生 CC_HealthBarWidget）。
	 *  留空使用 项目设置 → CC World Indicator 中的默认血条。软引用，专用服务器不会加载。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar", meta = (MustImplement = "/Script/GAS_Demo.CC_WorldIndicatorWidget"))
	TSoftClassPtr<UUserWidget> WidgetClass;
	/** 叠加在角色位置上的世界偏移（厘米），控件底边中点对齐这里。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar")
	FVector WorldOffset = FVector(0.f, 0.f, 80.f);
	/** 超过这个距离不显示（厘米）；0 表示不限。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Bar", meta = (ClampMin = "0"))
	float MaxDistance = 5000.f;
};


/** 一项初始技能配置。以后可在这里添加输入标签等“每个技能各不相同”的配置。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_CharacterAbilityEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSubclassOf<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (ClampMin = "1", UIMin = "1"))
	int32 AbilityLevel = 1;
};

/**
 * 角色的共享默认配置（原 CC_PawnData）。编辑器中配置，运行时按只读数据使用。
 * 这里只描述“角色拥有哪些配置”；授予能力和运行时状态由 Character / ASC 管理。
 * 后续可按职责增加动作集、输入配置等资产引用，不要把当前血量、技能句柄等实例状态放进来。
 */
UCLASS(BlueprintType)
class GAS_DEMO_API UCC_CharacterConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 保留配置顺序；空数组表示该角色不授予初始技能。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "Character|Abilities",
		meta = (TitleProperty = "Ability"))
	TArray<TSubclassOf<UCC_GameplayAbilityBase>> StartupAbilities;

	/** 只保存 GE 类；实际属性值仍由角色的 AttributeSet 管理。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Attributes")
	TSubclassOf<UGameplayEffect> InitializeAttributesEffect;

	/**
	 * 被玩家方击杀时给予的基础经验（生存模式会再乘以波次档位的倍率）。玩家角色填 0。
	 * 这是角色配置，不是 Buff 定义字段。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Progression", meta = (ClampMin = "0"))
	float ExperienceReward = 10.f;

	/** 该角色可用动作。每种怪物可以配置不同的 ActionSet。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Montage")
	TObjectPtr<UCombatActionSet> AttackActionSet;
	/** 该角色可用动作。每种怪物可以配置不同的 ActionSet。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Montage")
	TObjectPtr<UCombatActionData> DodgeActionData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Montage")
	TObjectPtr<UCombatActionData> BeHitActionData;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Montage")
	TObjectPtr<UCombatActionData> DeathActionData;

	/**
	 * 角色的基础阵营。为空时使用角色类的默认阵营（玩家角色为 CCTags.Team.Player，敌人为 CCTags.Team.Enemy）。
	 * 站在玩家一边的 AI 随从在这里填 CCTags.Team.Player。敌我判断统一走 UCC_TeamStatics::IsHostile。
	 * 同一个蓝图既当敌人又当召唤物时不要改这里，生成时用 ACC_BaseCharacter::SetTeamOverride 设置实例阵营。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Team", meta = (Categories = "CCTags.Team"))
	FGameplayTag Team;

	/** 旧索敌方案（CC_FindRangedTarget）使用的 Actor Tag；AI 控制器已改为按阵营选目标，不再读取它。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Character|Targeting")
	FName TargetingActorTag = FName("Player");
	/** 最大射程；目标距离不超过它时无需移动。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|Ranged")
	float MaxRange = 850.0f;
	/** 最小射程；目标比它更近时视为不在射程内，需要移动拉开距离。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|Ranged")
	float MinRange = 0.0f;
	/** 仅弓兵配置：箭 Actor 类。为空时继续使用原来的近战攻击能力。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|Ranged")
	TSubclassOf<ACC_ArrowProjectile> ArrowProjectileClass;
	/** 放箭相对蒙太奇开始的时间（秒），用于让生成箭的时刻对齐动画。 */
	// UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|Ranged", meta = (ClampMin = "0"))
	// float ArrowReleaseTime = 0.35f;
	/** 优先从此 Socket 出箭；角色骨骼没有该 Socket 时使用身前的保底位置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|Ranged")
	FName ArrowSocketName = FName("ArrowSocket");
	/** 箭的初速度，按当前目标位置计算发射方向。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|Ranged", meta = (ClampMin = "1"))
	float ArrowSpeed = 1800.0f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Crash|AI")
	float AttackDelay{1.0f};

	/**
	 * 近战攻击距离（厘米），按双方胶囊体边缘计算，不是中心距离。
	 * 近战的 FollowTarget 走到这个距离内即停下；行为树判断「在攻击距离内」与控制器的被挡检测也用它。
	 * 按 UCC_Primary::HitBoxRadius 和攻击蒙太奇的前冲距离调整。弓兵不使用。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crash|AI", meta = (ClampMin = "0"))
	float MeleeReach = 80.0f;

	/** 是否显示头顶血条。本地玩家自己的角色始终不显示（自身血量在 HUD 上）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|UI")
	bool bShowOverheadHealthBar = true;
	/** 头顶血条用哪个控件、挂在哪、多远隐藏；满血 / 死亡隐藏在血条控件上配置。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|UI", meta = (EditCondition = "bShowOverheadHealthBar"))
	FCC_OverheadHealthBarConfig OverheadHealthBar;
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
