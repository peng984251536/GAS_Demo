#pragma once

#include "CoreMinimal.h"
#include "CC_CombatTypes.generated.h"

/** 玩家生命/法力的一次完整展示快照，保证当前值和最大值一起发布。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_PlayerVitals
{
	GENERATED_BODY()
	/** 当前生命值。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat") float Health = 0.f;
	/** 最大生命值，0 时界面应显示空进度。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat") float MaxHealth = 0.f;
	/** 当前法力值。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat") float Mana = 0.f;
	/** 最大法力值。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Combat") float MaxMana = 0.f;

	/** 生命比例，已钳制到 [0,1]；最大值无效时为 0。 */
	float GetHealthPercent() const { return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f; }
	/** 法力比例，已钳制到 [0,1]；最大值无效时为 0。 */
	float GetManaPercent() const { return MaxMana > 0.f ? FMath::Clamp(Mana / MaxMana, 0.f, 1.f) : 0.f; }

	bool operator==(const FCC_PlayerVitals& Other) const
	{
		return Health == Other.Health && MaxHealth == Other.MaxHealth && Mana == Other.Mana && MaxMana == Other.MaxMana;
	}
	bool operator!=(const FCC_PlayerVitals& Other) const { return !(*this == Other); }
};
