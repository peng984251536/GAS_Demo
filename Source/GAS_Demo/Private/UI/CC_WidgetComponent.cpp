// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/CC_WidgetComponent.h"
#include "GAS_Demo.h"

#include "AbilitySystem/CC_AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "UI/CC_AttributeWidget.h"


// 保留世界空间 UI 绘制所需的父类 Tick，不用 Tick 轮询属性。
UCC_WidgetComponent::UCC_WidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
}

/**
 * 初始化
 */
void UCC_WidgetComponent::InitWidget()
{
	// 负责根据widgetClass 创建ui
	Super::InitWidget();
	if (IsASCInitialized() && AttributeSet->bAttributesInitialized) BindToAttributeChanges();
}

/**
 * 初始化-拿到 角色、属性、ASC属性
 */
bool UCC_WidgetComponent::InitAbilitySystemData()
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	CrashCharacter = Cast<ACC_BaseCharacter>(Owner);
	
	if (!CrashCharacter.IsValid()) return false;
	AttributeSet = Cast<UCC_AttributeSet>(CrashCharacter->GetAttributeSet());
	AbilitySystemComponent = Cast<UCC_AbilitySystemComponent>(
		CrashCharacter->GetAbilitySystemComponent());

	return IsASCInitialized();
}

/**
 * 只有 ASC 和 AS 初始化后才返回true
 * @return 
 */
bool UCC_WidgetComponent::IsASCInitialized() const
{
	return AbilitySystemComponent.IsValid() && AttributeSet.IsValid();
}

/**
 * 拿到属性的某个值
 * @param Attribute 
 * @return 
 */
float UCC_WidgetComponent::GetAttributeValue(
	const FGameplayAttribute& Attribute) const
{
	if (!IsASCInitialized())
	{
		return 0.f;
	}

	return AbilitySystemComponent
		->GetNumericAttribute(Attribute);
}

#pragma region 第一层延迟

// 尝试获取当前数据源，同时支持 ASC 晚于组件初始化。
void UCC_WidgetComponent::BeginPlay()
{
	Super::BeginPlay();

	//尝试获取一次数据 1
	InitAbilitySystemData();
	if (CrashCharacter.IsValid()) CrashCharacter->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::OnASCInitialized);
	if (IsASCInitialized()) InitializeAttributeDelegate();
}

// 结束时清理两级初始化事件和所有属性变化订阅。
void UCC_WidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAttributeDelegates();
	
	if (UCC_AttributeSet* AS = AttributeSet.Get())
	{
		AS->OnAttributesInitialized.RemoveDynamic(
			this, &ThisClass::BindToAttributeChanges);
	}

	if (ACC_BaseCharacter* Character = CrashCharacter.Get())
	{
		Character->OnASCInitialized.RemoveDynamic(
			this, &ThisClass::OnASCInitialized);
	}
	
	Super::EndPlay(EndPlayReason);
	
}

/**
 * //尝试获取一次数据 2
 * @param ASC 
 * @param AS 
 */
void UCC_WidgetComponent::OnASCInitialized(UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	UnbindAttributeDelegates();
	if (AttributeSet.IsValid()) AttributeSet->OnAttributesInitialized.RemoveDynamic(this, &ThisClass::BindToAttributeChanges);
	AbilitySystemComponent = Cast<UCC_AbilitySystemComponent>(ASC);
	AttributeSet = Cast<UCC_AttributeSet>(AS);

	InitializeAttributeDelegate();
}

#pragma endregion 

#pragma region 第二层延迟

/**
 * 等属性初始化后，绑定属性
 */
void UCC_WidgetComponent::InitializeAttributeDelegate()
{
	if (!IsASCInitialized()) return;
	//如果属性还没初始化，就等
	if (!AttributeSet->bAttributesInitialized)
	{
		// 等属性初始化完成的委托触发后再绑定
		AttributeSet->OnAttributesInitialized.AddUniqueDynamic(this,
			&ThisClass::BindToAttributeChanges);
	}
	// 属性已经初始化了，就直接绑定
	else
	{
		BindToAttributeChanges();
	}
}

/**
 * 遍历属性映射和 Widget 树
 */
void UCC_WidgetComponent::BindToAttributeChanges()
{
	UnbindAttributeDelegates();
	if (!IsASCInitialized()) return;
	// 遍历属性表
	for(const TTuple<FGameplayAttribute,FGameplayAttribute>& Pair:AttributeMap)
	{
		UUserWidget* UserWidget = GetUserWidgetObject();
		if (!IsValid(UserWidget) || !IsValid(UserWidget->WidgetTree))
		{
			UE_LOG(
				LogGAS_Demo,
				Warning,
				TEXT("Attribute widget is not ready on component %s; InitWidget will retry."),
				*GetNameSafe(this));
			return;
		}

		
		if (!Pair.Key.IsValid() || !Pair.Value.IsValid()) continue;
		BindWidgetToAttributeChanges(UserWidget,Pair);

		// 遍历其中所有子 Widget。
		GetUserWidgetObject()->WidgetTree->ForEachWidgetAndDescendants(
				[this, &Pair]
				(UWidget* ChildWidget)
				{
					BindWidgetToAttributeChanges(ChildWidget,Pair);
				}
			);
	}
}

/**
 * 工具类，通知ui属性发生变化
 * @param WidgetObject 
 * @param Pair 
 */
void UCC_WidgetComponent::BindWidgetToAttributeChanges(UWidget* WidgetObject,
                                                       const TTuple<FGameplayAttribute, FGameplayAttribute>& Pair)
{
	UCC_AttributeWidget* AttributeWidget = Cast<UCC_AttributeWidget>(WidgetObject);
	
	// 只处理属性 Widget。
	if (!IsValid(AttributeWidget))
	{
		return;
	}
	// 只订阅与该 Widget 配置匹配的属性对。
	if (!AttributeWidget->MatchesAttributes(Pair))
	{
		return;
	}

	// 先初始化一次值
	AttributeWidget->OnAttributeChange(
		Pair,
		AttributeSet.Get());


	// 监听游戏过程中当前属性的变化。
	// 监听变化的委托
	const TWeakObjectPtr<UCC_AttributeWidget> WeakWidget = AttributeWidget;
	TSet<FGameplayAttribute> ObservedAttributes = {Pair.Key, Pair.Value};
	for (const FGameplayAttribute& ObservedAttribute : ObservedAttributes)
	{
		if (!ObservedAttribute.IsValid()) continue;
		FDelegateHandle Handle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(ObservedAttribute).AddWeakLambda(this,
		[this, WeakWidget, Pair]
		(const FOnAttributeChangeData&)
		{
			if (!WeakWidget.IsValid() ||
				!AttributeSet.IsValid())
			{
				return;
			}

			WeakWidget->OnAttributeChange(
				Pair,
				AttributeSet.Get());
		}
	);
		AttributeDelegateHandles.Emplace(ObservedAttribute, Handle);
	}
	
}

// 只清理本组件保存的句柄，不删除其他 UI 的委托。
void UCC_WidgetComponent::UnbindAttributeDelegates()
{
	if (AbilitySystemComponent.IsValid())
		for (const auto& Entry : AttributeDelegateHandles)
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Entry.Key).Remove(Entry.Value);
	AttributeDelegateHandles.Reset();
}

#pragma endregion 


