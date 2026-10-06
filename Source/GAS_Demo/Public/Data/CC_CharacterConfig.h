#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/CC_GameplayAbilityBase.h"
#include "Engine/DataAsset.h"
#include "CC_CharacterConfig.generated.h"

class UGameplayAbility;
class UGameplayEffect;
class UCombatActionSet;
class UCombatActionData;
class ACC_ArrowProjectile;


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

	/** 索敌时查找的 Actor Tag；带该 Tag 的存活角色会成为目标。默认 Player，改它即可改变索敌目标。 */
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
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
