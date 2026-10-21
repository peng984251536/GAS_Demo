#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "CC_StatTagStack.generated.h"

/** 一项统计：标签 → 计数。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_StatTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	FGameplayTag Tag;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 StackCount = 0;
};

/**
 * 统计容器（参考 Lyra 的 FGameplayTagStackContainer）。用 FastArray 增量复制；
 * 新增统计只需新增标签，不改数据结构，结算页遍历即可显示。只在服务器修改。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_StatTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

	/** 累加（Count 可为负，结果不低于 0）。无效标签或 0 忽略。 */
	void AddStack(FGameplayTag Tag, int32 Count);

	/** 读取计数；不存在时为 0。 */
	int32 GetStackCount(FGameplayTag Tag) const;

	/** 清空全部统计。 */
	void Reset();

	const TArray<FCC_StatTagStack>& GetStacks() const { return Stacks; }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FCC_StatTagStack, FCC_StatTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:
	UPROPERTY()
	TArray<FCC_StatTagStack> Stacks;
};

template<>
struct TStructOpsTypeTraits<FCC_StatTagStackContainer> : public TStructOpsTypeTraitsBase2<FCC_StatTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
