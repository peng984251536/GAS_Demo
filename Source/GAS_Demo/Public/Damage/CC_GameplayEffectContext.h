#pragma once

#include "GameplayEffectTypes.h"
#include "AbilitySystemGlobals.h"
#include "Damage/CC_DamageTypes.h"
#include "CC_GameplayEffectContext.generated.h"

/**
 * 项目 GE Context：在基类（来源、命中结果等）之上只加暴击信息。
 * 伤害类型和来源标签不放这里，它们写在 Spec 标签上，GAS 会一并传给修正器、GameplayEvent 和 Cue。
 */
USTRUCT()
struct GAS_DEMO_API FCC_GameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()
	/** 暴击策略，只在服务器使用，不复制。 */
	ECC_CriticalPolicy CriticalPolicy = ECC_CriticalPolicy::Disabled;
	/** 服务器 Execution 判定的结果，复制给客户端用于显示。 */
	bool bCritical = false;

	/** 句柄里是项目 Context 时返回它，否则返回 nullptr。 */
	static const FCC_GameplayEffectContext* Get(const FGameplayEffectContextHandle& Handle);
	static FCC_GameplayEffectContext* GetMutable(FGameplayEffectContextHandle& Handle);

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	/** 必须重写以复制出派生类型；HitResult 深复制，避免多个命中共用一份。 */
	virtual FGameplayEffectContext* Duplicate() const override
	{
		FCC_GameplayEffectContext* Copy = new FCC_GameplayEffectContext(*this);
		if (GetHitResult()) Copy->AddHitResult(*GetHitResult(), true);
		return Copy;
	}
	/** 基类数据之外只多序列化 1 位暴击标记。 */
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};

template <>
struct TStructOpsTypeTraits<FCC_GameplayEffectContext> : TStructOpsTypeTraitsBase2<FCC_GameplayEffectContext>
{
	enum { WithNetSerializer = true, WithCopy = true };
};

/** 让 ASC::MakeEffectContext 创建项目 Context；在 DefaultGame.ini 中配置，更换后需重启编辑器。 */
UCLASS()
class GAS_DEMO_API UCC_AbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
public:
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override { return new FCC_GameplayEffectContext(); }
};
