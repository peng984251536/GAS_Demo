#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GameplayTagContainer.h"
#include "UI/WorldIndicator/CC_WorldIndicator.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageNumberSource.generated.h"

/** 一个伤害 / 治疗数字的指示器：通用字段之外带上数值和样式，数字控件在 Bind 时读取。 */
UCLASS()
class GAS_DEMO_API UCC_DamageNumberIndicator : public UCC_WorldIndicator
{
	GENERATED_BODY()
public:
	/** 显示的数值（正数）。 */
	UPROPERTY(BlueprintReadOnly, Category="Damage Number") float Amount = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Damage Number") ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal;
	/** 伤害类型（Damage.Type.*），可供控件选颜色或图标。 */
	UPROPERTY(BlueprintReadOnly, Category="Damage Number") FGameplayTag DamageType;
	/** 受击 / 被治疗的角色，用于同一目标多个数字的错位；数字本身固定在出现位置，不跟随它。 */
	UPROPERTY() TWeakObjectPtr<AActor> Target;
};

/**
 * 伤害数字的数据来源（每个本地玩家一份）。由 DamageText Cue 调用 ShowNumber，
 * 为数字创建一个固定坐标的指示器；数字控件播完动画后自己移除，这里回收指示器对象复用。
 * 负责：同一目标多个数字的左右错位、每个玩家的数量上限（满时先移除最旧的普通数字）。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageNumberSource : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	/** 取本地玩家的伤害数字来源；不是本地玩家时返回 nullptr。 */
	static UCC_DamageNumberSource* GetForPlayer(const APlayerController* PlayerController);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 在世界坐标 Location 显示一个数字。Amount 取绝对值；治疗请用 Heal 样式。 */
	void ShowNumber(AActor* Target, const FVector& Location, float Amount, ECC_DamageTextStyle Style, FGameplayTag DamageType);
	/** 当前显示中的数字数量，供调试和测试。 */
	int32 GetActiveCount() const { return Active.Num(); }
	const TArray<TObjectPtr<UCC_DamageNumberIndicator>>& GetActiveNumbers() const { return Active; }
private:
	void HandleIndicatorRemoved(UCC_WorldIndicator* Indicator);

	/** 显示中的数字，按出现顺序（最旧在前）。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UCC_DamageNumberIndicator>> Active;
	/** 可复用的指示器对象，避免每次命中都分配 UObject。 */
	UPROPERTY(Transient) TArray<TObjectPtr<UCC_DamageNumberIndicator>> Free;
	FDelegateHandle RemovedHandle;
};
