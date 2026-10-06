// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/CC_AttributeSet.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/WidgetComponent.h"

#include "CC_WidgetComponent.generated.h"

class ACC_BaseCharacter;
class UCC_AbilitySystemComponent;
class UCC_AttributeSet;
class UCC_AttributeWidget;

/** 世界空间属性条适配器：等待 ASC 就绪，通过事件更新普通 UMG，不加入 CommonUI 栈。 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_WidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	/** 保留父类绘制世界空间 UI 所需的 Tick，属性数据本身通过事件更新。 */
	UCC_WidgetComponent();

	/** 控件晚于 ASC 创建时，补做初值推送和属性绑定。 */
	virtual void InitWidget() override;
	/** 读取当前 ASC 的指定属性，数据源尚未就绪时返回 0。 */
	virtual float GetAttributeValue(const FGameplayAttribute& Attribute) const;
protected:
	/** 当前属性 → 最大属性，例如 Health → MaxHealth。 */
	UPROPERTY(EditAnywhere, Category = "GAS|UI")
	TMap<FGameplayAttribute, FGameplayAttribute> AttributeMap;

	/** 开始运行时尝试绑定属性，并订阅角色后续的 ASC 初始化通知。 */
	virtual void BeginPlay() override;
	/** 清除角色、属性初始化及属性变化委托，避免结束后残留回调。 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** 所属角色的弱引用，不延长其生命周期。 */
	TWeakObjectPtr<ACC_BaseCharacter> CrashCharacter;
	/** 正在监听的能力系统；重新绑定前先移除旧系统的委托。 */
	TWeakObjectPtr<UCC_AbilitySystemComponent> AbilitySystemComponent;
	/** 属性值和初始化状态的数据来源。 */
	TWeakObjectPtr<UCC_AttributeSet> AttributeSet;
	
	/** 尝试从所属角色取得 ASC 和属性集。 */
	bool InitAbilitySystemData();
	/** ASC 和属性集是否均有效。 */
	bool IsASCInitialized() const;
	

	/** 角色通知 ASC 就绪时，先解绑旧数据源，再等待新属性集初始化。 */
	UFUNCTION()
	void OnASCInitialized(UAbilitySystemComponent* ASC, UAttributeSet* AS);

	/** 属性已初始化则立即绑定，否则订阅属性初始化完成事件。 */
	void InitializeAttributeDelegate();
	/** 遍历属性映射及控件树，推送初值并建立属性订阅。 */
	UFUNCTION()
	void BindToAttributeChanges();

	/** 匹配控件配置，推送初值，并同时监听当前值与最大值。 */
	void BindWidgetToAttributeChanges(UWidget* WidgetObject,
		const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair);
	/** 按句柄移除本组件创建的全部属性委托，重复调用安全。 */
	void UnbindAttributeDelegates();
	/** 保存每项属性的订阅句柄，防止重复绑定和组件销毁后残留。 */
	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeDelegateHandles;
};
