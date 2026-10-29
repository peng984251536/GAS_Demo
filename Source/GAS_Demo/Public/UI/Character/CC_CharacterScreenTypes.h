#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "CC_CharacterScreenTypes.generated.h"

class UGameplayEffect;
class UTexture2D;
class UWorld;

/** 属性数值的显示方式；只影响文字，不改变数值本身。 */
UENUM(BlueprintType)
enum class ECC_StatDisplayFormat : uint8
{
	Integer,  // 148
	Decimal,  // 1.5
	Percent,  // 0.25 → 25%
};

/** 控制器配置：角色面板要显示哪个属性、叫什么名字、怎么格式化。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_CharacterStatConfig
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Character") FGameplayAttribute Attribute;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Character") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Character") ECC_StatDisplayFormat Format = ECC_StatDisplayFormat::Integer;
};

/** 左侧属性列表的一行：数值和已格式化好的文字。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_CharacterStatEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="UI|Character") FGameplayAttribute Attribute;
	UPROPERTY(BlueprintReadOnly, Category="UI|Character") FText Label;
	UPROPERTY(BlueprintReadOnly, Category="UI|Character") float Value = 0.f;
	/** 按配置格式化后的文字，WBP 直接显示即可。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Character") FText ValueText;

	bool operator==(const FCC_CharacterStatEntry& Other) const
	{
		return Attribute == Other.Attribute && Value == Other.Value && Label.EqualTo(Other.Label);
	}
	bool operator!=(const FCC_CharacterStatEntry& Other) const { return !(*this == Other); }
};

/**
 * 右侧 Buff 列表的一条，由 FCC_ReplicatedBuffItem + GE 上的 UCC_BuffUIData 合成。
 * 剩余时间不存这里，界面用 EndServerTime 和服务器时钟实时计算（与 CC_BuffBarWidget 一致）。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_CharacterBuffEntry
{
	GENERATED_BODY()
	/** BuffComponent 分配的实例号，同一个 ActiveGE 叠层、刷新时保持不变。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") int32 InstanceId = 0;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") TSubclassOf<UGameplayEffect> EffectClass;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") FText Description;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") TObjectPtr<UTexture2D> Icon;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") int32 StackCount = 1;
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") float Level = 1.f;
	/** 带有控制器配置的 DebuffTag 时为 true；未配置时始终为 false。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") bool bIsDebuff = false;
	/** 被抑制：GE 还在但暂时不生效，界面可半透明显示。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") bool bIsInhibited = false;
	/** GE 总时长；负数为无限期。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") float Duration = -1.f;
	/** 服务器时钟上的到期时刻；负数为无限期。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") float EndServerTime = -1.f;
	/** 资产标签 + 授予标签，供 WBP 按需分类。 */
	UPROPERTY(BlueprintReadOnly, Category="UI|Buff") FGameplayTagContainer BuffTags;

	bool IsInfinite() const { return Duration < 0.f || EndServerTime < 0.f; }

	bool operator==(const FCC_CharacterBuffEntry& Other) const
	{
		return InstanceId == Other.InstanceId && StackCount == Other.StackCount && Level == Other.Level
			&& bIsDebuff == Other.bIsDebuff && bIsInhibited == Other.bIsInhibited && Duration == Other.Duration
			&& FMath::IsNearlyEqual(EndServerTime, Other.EndServerTime, 0.01f);
	}
	bool operator!=(const FCC_CharacterBuffEntry& Other) const { return !(*this == Other); }
};

namespace CCCharacterScreen
{
	/** 与 UCC_BuffComponent::ServerTime 相同：优先用 GameState 的服务器时钟，单机暂停时一起冻结。 */
	GAS_DEMO_API float GetServerTime(const UWorld* World);
	/** 剩余秒数；无限期返回 -1，已到期返回 0。 */
	GAS_DEMO_API float GetRemainingTime(const UWorld* World, const FCC_CharacterBuffEntry& Buff);
}
