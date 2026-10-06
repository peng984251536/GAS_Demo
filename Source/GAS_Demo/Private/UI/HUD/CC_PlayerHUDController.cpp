#include "UI/HUD/CC_PlayerHUDController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Character/CC_BaseCharacter.h"
#include "GameFramework/PlayerController.h"

void UCC_PlayerHUDModel::SetVitals(const FCC_PlayerVitals& Value)
{
	if (Vitals.Health == Value.Health && Vitals.MaxHealth == Value.MaxHealth && Vitals.Mana == Value.Mana && Vitals.MaxMana == Value.MaxMana) return;
	Vitals = Value;
	NotifyChanged();
}

UCC_PlayerHUDController::UCC_PlayerHUDController() { ModelClass = UCC_PlayerHUDModel::StaticClass(); }

void UCC_PlayerHUDController::OnActivated()
{
	if (APlayerController* Player = GetPlayerController())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		HandlePawnChanged(nullptr, Player->GetPawn());
	}
	Super::OnActivated();
}

void UCC_PlayerHUDController::OnDeactivated()
{
	if (APlayerController* Player = GetPlayerController()) Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	UnbindPawn();
	Super::OnDeactivated();
}

// Pawn 变化时替换数据源，ASC 尚未就绪则等待通知。
void UCC_PlayerHUDController::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UnbindPawn();
	BoundCharacter = Cast<ACC_BaseCharacter>(NewPawn);
	if (BoundCharacter.IsValid()) BoundCharacter->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleASCReady);
	BindASC(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewPawn));
}
// 角色延迟初始化完成后，把 HUD 绑定到新的能力系统。
void UCC_PlayerHUDController::HandleASCReady(UAbilitySystemComponent* NewASC, UAttributeSet* Attributes) { BindASC(NewASC); }
// 同时订阅当前值和最大值，绑定后主动推送初始快照。
void UCC_PlayerHUDController::BindASC(UAbilitySystemComponent* NewASC)
{
	UnbindASC();
	BoundASC = NewASC;
	if (NewASC)
	{
		for (FGameplayAttribute Attribute :{
				UCC_AttributeSet::GetHealthAttribute(),
				UCC_AttributeSet::GetMaxHealthAttribute(),
				UCC_AttributeSet::GetManaAttribute(),
				UCC_AttributeSet::GetMaxManaAttribute()})
		{
			AttributeHandles.Add(Attribute, NewASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged));
		}
	}
	Refresh();
}
// 按句柄移除本 HUD 的订阅，不影响其他观察者。
void UCC_PlayerHUDController::UnbindASC()
{
	if (BoundASC.IsValid()) for (const auto& Pair : AttributeHandles) BoundASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
	AttributeHandles.Reset();
	BoundASC.Reset();
}
// 一并解除角色初始化事件和属性事件。
void UCC_PlayerHUDController::UnbindPawn()
{
	if (BoundCharacter.IsValid()) BoundCharacter->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleASCReady);
	BoundCharacter.Reset();
	UnbindASC();
}
// 任意一项属性变化都刷新整组展示，保持当前值和最大值一致。
void UCC_PlayerHUDController::HandleAttributeChanged(const FOnAttributeChangeData& Data) { Refresh(); }
// 读取当前快照、安全计算进度比例，并通知蓝图展示层。
void UCC_PlayerHUDController::Refresh()
{
	if (!IsActive()) return;
	// 缺少数据源时发布零值，角色切换期间不保留旧角色数值。
	const auto Read = [this](FGameplayAttribute Attribute) { return BoundASC.IsValid() ? BoundASC->GetNumericAttribute(Attribute) : 0.f; };
	FCC_PlayerVitals Value;
	Value.Health = Read(UCC_AttributeSet::GetHealthAttribute());
	Value.MaxHealth = Read(UCC_AttributeSet::GetMaxHealthAttribute());
	Value.Mana = Read(UCC_AttributeSet::GetManaAttribute());
	Value.MaxMana = Read(UCC_AttributeSet::GetMaxManaAttribute());
	if (UCC_PlayerHUDModel* HUDModel = Cast<UCC_PlayerHUDModel>(GetModel()))
	{
		HUDModel->SetVitals(Value);
	}
		
}
