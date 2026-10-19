#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "CC_AttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAttributesInitialized);

/**
 * 保存角色资源和战斗属性，统一限制数值范围，并消费 Damage / Healing 中间属性完成扣血和治疗。
 * 伤害公式在 UCC_DamageExecution；扣血后在服务器发送 Event.Combat.DamageDealt / DamageTaken。
 * 这里不做任何表现（飘字由伤害 GE 上的 GameplayCue 负责）。
 */
UCLASS()
class GAS_DEMO_API UCC_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/**
	 * 设置属性兜底初值，角色初始化 GE 可覆盖这些基础值。
	 */
	UCC_AttributeSet();

	// 属性变化与 GE 结算

	/**
	 * 登记需要复制的持久属性；伤害和治疗中转值不复制。
	 * @param OutLifetimeProps 输出的属性复制规则列表。
	 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 基础属性值变化前限制合法范围。
	 * @param Attribute 正在发生变化的属性字段标识。
	 * @param NewValue 待写入的数值，按引用裁剪到合法范围。
	 */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

	/**
	 * 当前属性值变化前限制合法范围。
	 * @param Attribute 正在发生变化的属性字段标识。
	 * @param NewValue 待写入的数值，按引用裁剪到合法范围。
	 */
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/**
	 * 消费 Damage / Healing 中间属性修改生命；伤害后发送 GameplayEvent。同时维护资源边界。
	 * @param Data 本次 GE 的目标、Spec 和属性修改数据。
	 */
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// 初始化状态

	/**
	 * 属性初始化标记；复制规则与其他通知属性保持 REPNOTIFY_Always。
	 */
	UPROPERTY(ReplicatedUsing = OnRep_AttributesInitialized)
	bool bAttributesInitialized = false;

	/**
	 * 属性首次初始化或初始化标记复制到客户端后通知观察者。
	 */
	UPROPERTY(BlueprintAssignable)
	FAttributesInitialized OnAttributesInitialized;

	// 基础资源与攻击属性

	/** 当前生命值；伤害和治疗通过中转属性结算后修改。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Vital")
	FGameplayAttributeData Health;
	/** 最大生命值，限制当前生命值上限。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	/** 当前法力值。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Attributes|Vital")
	FGameplayAttributeData Mana;
	/** 最大法力值，限制当前法力值上限。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Attributes|Vital")
	FGameplayAttributeData MaxMana;
	/** 攻击力数值；GAS 捕获时包含已聚合的攻击 Buff。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_AttackPower, Category = "Attributes|Vital")
	FGameplayAttributeData AttackPower;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MoveSpeedMultiplier, Category="Attributes|Combat")
	FGameplayAttributeData MoveSpeedMultiplier;
	// 伤害修正与暴击属性
	/** 固定防御，从防御前伤害中扣除，不是百分比护甲。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense, Category = "Attributes|Combat")
	FGameplayAttributeData Defense;
	/** 额外增伤比例：0 为无增伤，0.5 表示增伤 50%。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_OutgoingDamageBonus, Category = "Attributes|Combat")
	FGameplayAttributeData OutgoingDamageBonus;
	/** 承伤倍率：1 为正常，0.8 表示减伤 20%，1.2 表示易伤 20%。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_IncomingDamageMultiplier, Category = "Attributes|Combat")
	FGameplayAttributeData IncomingDamageMultiplier;
	/** 暴击概率，范围 0..1；0.25 表示 25%。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalChance, Category = "Attributes|Combat")
	FGameplayAttributeData CriticalChance;
	/** 暴击总倍率；1.5 表示暴击伤害为普通伤害的 150%。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CriticalMultiplier, Category = "Attributes|Combat")
	FGameplayAttributeData CriticalMultiplier;

	// 一次性中间属性（不复制）：只由伤害/治疗 GE 写入，消费后立即清零，不要配置持续修正
	/**
	 * 本次伤害量，只由 UCC_DamageExecution 输出；伤害 GE 上的飘字 Cue 读它。
	 * 在 GE 修正器列表中隐藏，策划无法绕过 UCC_DamageStatics 直接配置扣血（CLAUDE.md 规则 3）。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta", meta = (HideFromModifiers))
	FGameplayAttributeData Damage;
	/** 本次治疗量，由治疗 GE 写入（药水等 GE 也可以直接配置它）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData Healing;

	// 属性复制回调

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttackPower(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_OutgoingDamageBonus(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_CriticalChance(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_CriticalMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_AttributesInitialized();

	// 属性访问器：统一集中声明 Getter / Setter / Init

	ATTRIBUTE_ACCESSORS(ThisClass, MoveSpeedMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Health);
	ATTRIBUTE_ACCESSORS(ThisClass, MaxHealth);
	ATTRIBUTE_ACCESSORS(ThisClass, Mana);
	ATTRIBUTE_ACCESSORS(ThisClass, MaxMana);
	ATTRIBUTE_ACCESSORS(ThisClass, AttackPower);
	ATTRIBUTE_ACCESSORS(ThisClass, Defense);
	ATTRIBUTE_ACCESSORS(ThisClass, OutgoingDamageBonus);
	ATTRIBUTE_ACCESSORS(ThisClass, IncomingDamageMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, CriticalChance);
	ATTRIBUTE_ACCESSORS(ThisClass, CriticalMultiplier);
	ATTRIBUTE_ACCESSORS(ThisClass, Damage);
	ATTRIBUTE_ACCESSORS(ThisClass, Healing);

private:
	/**
	 * 按属性类型统一限制数值范围，不重复执行伤害计算。
	 * @param Attribute 待限制的属性字段。
	 * @param Value 待写入的值，按引用原地修改。
	 */
	void ClampAttribute(const FGameplayAttribute& Attribute, float& Value) const;
};
