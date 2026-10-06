// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/CC_AttributeChangeTask.h"
#include "AbilitySystemComponent.h"

/**
 * 为 ASC 创建一个Task(这是个静态函数)
 * @param AbilitySystemComponent 
 * @param Attribute 
 * @return 
 */
UCC_AttributeChangeTask* UCC_AttributeChangeTask::ListenForAttributeChange(
	UAbilitySystemComponent* AbilitySystemComponent,
	FGameplayAttribute Attribute)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return nullptr;
	}
	
	UCC_AttributeChangeTask* Task = NewObject<UCC_AttributeChangeTask>();
	Task->ASC = AbilitySystemComponent;
	Task->AttributeToListenFor = Attribute;

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute)
	.AddUObject(
		Task,
		&UCC_AttributeChangeTask::AttributeChanged);
	
	
	return Task;
}

void UCC_AttributeChangeTask::AttributeChanged(const FOnAttributeChangeData& Data) const
{
	OnAttributeChanged.Broadcast(
		Data.Attribute,
		Data.NewValue,
		Data.OldValue);
}


void UCC_AttributeChangeTask::EndTask()
{
	if(ASC.IsValid())
	{
		FOnGameplayAttributeValueChange& ChangeDelegate = ASC->
		GetGameplayAttributeValueChangeDelegate(AttributeToListenFor);
		
		ChangeDelegate.RemoveAll(this);
	}

	// 异步任务已经结束，不需要 GameInstance保活
	// 配合 RegisterWithGameInstance(...)
	SetReadyToDestroy();
	// 把这个UObject标记为垃圾对象，等GC清除。强制宣布对象已废弃；这个场景通常多余，建议不调用
	// MarkAsGarbage();
}





















