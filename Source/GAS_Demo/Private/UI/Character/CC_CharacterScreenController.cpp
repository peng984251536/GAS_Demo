#include "UI/Character/CC_CharacterScreenController.h"
#include "UI/Character/CC_CharacterScreenModel.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Attribute/CC_AttributeSet.h"
#include "Character/CC_BaseCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffUIData.h"
#include "GameplayEffect.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "CCCharacterScreen"

UCC_CharacterScreenController::UCC_CharacterScreenController()
{
	ModelClass = UCC_CharacterScreenModel::StaticClass();
	const auto Add = [this](const FGameplayAttribute& Attribute, const FText& Label, ECC_StatDisplayFormat Format)
	{
		FCC_CharacterStatConfig& Config = DisplayedStats.AddDefaulted_GetRef();
		Config.Attribute = Attribute;
		Config.Label = Label;
		Config.Format = Format;
	};
	Add(UCC_AttributeSet::GetAttackPowerAttribute(), LOCTEXT("AttackPower", "攻击力"), ECC_StatDisplayFormat::Integer);
	Add(UCC_AttributeSet::GetDefenseAttribute(), LOCTEXT("Defense", "防御"), ECC_StatDisplayFormat::Integer);
	Add(UCC_AttributeSet::GetCriticalChanceAttribute(), LOCTEXT("CriticalChance", "暴击率"), ECC_StatDisplayFormat::Percent);
	Add(UCC_AttributeSet::GetCriticalMultiplierAttribute(), LOCTEXT("CriticalMultiplier", "暴击伤害"), ECC_StatDisplayFormat::Percent);
	Add(UCC_AttributeSet::GetMoveSpeedMultiplierAttribute(), LOCTEXT("MoveSpeed", "移动速度"), ECC_StatDisplayFormat::Percent);
}

void UCC_CharacterScreenController::OnActivated()
{
	if (APlayerController* Player = GetPlayerController())
	{
		Player->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
		HandlePawnChanged(nullptr, Player->GetPawn());
	}
	Super::OnActivated();
}

void UCC_CharacterScreenController::OnDeactivated()
{
	if (APlayerController* Player = GetPlayerController()) Player->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
	UnbindPawn();
	Super::OnDeactivated();
}

// ---------------------------------------------------------------------------
// 数据源生命周期
// ---------------------------------------------------------------------------

// Pawn 变化时替换数据源。BuffComponent 是角色构造时创建的默认子对象，可以立即绑定；
// ASC 可能晚于 Pawn 就绪，等 OnASCInitialized 再绑定属性。
void UCC_CharacterScreenController::HandlePawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	UnbindPawn();
	BoundCharacter = Cast<ACC_BaseCharacter>(NewPawn);
	if (BoundCharacter.IsValid())
	{
		BoundCharacter->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleASCReady);
		BindBuffComponent(BoundCharacter->GetBuffComponent());
	}
	else
	{
		RefreshBuffs(); // 没有角色时发布空列表，不残留旧数据。
	}
	BindASC(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NewPawn));
}

void UCC_CharacterScreenController::HandleASCReady(UAbilitySystemComponent* NewASC, UAttributeSet* Attributes) { BindASC(NewASC); }

void UCC_CharacterScreenController::BindASC(UAbilitySystemComponent* NewASC)
{
	UnbindASC();
	BoundASC = NewASC;
	if (NewASC)
	{
		TArray<FGameplayAttribute> Watched = {
			UCC_AttributeSet::GetHealthAttribute(),
			UCC_AttributeSet::GetMaxHealthAttribute(),
			UCC_AttributeSet::GetManaAttribute(),
			UCC_AttributeSet::GetMaxManaAttribute()};
		for (const FCC_CharacterStatConfig& Config : DisplayedStats)
		{
			if (Config.Attribute.IsValid()) Watched.AddUnique(Config.Attribute);
		}
		for (const FGameplayAttribute& Attribute : Watched)
		{
			AttributeHandles.Add(Attribute, NewASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged));
		}
	}
	RefreshStats();
}

// 按句柄移除，不影响战斗主界面等其他观察同一 ASC 的对象。
void UCC_CharacterScreenController::UnbindASC()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		for (const auto& Pair : AttributeHandles) ASC->GetGameplayAttributeValueChangeDelegate(Pair.Key).Remove(Pair.Value);
	}
	AttributeHandles.Reset();
	BoundASC.Reset();
}

void UCC_CharacterScreenController::BindBuffComponent(UCC_BuffComponent* NewBuffs)
{
	UnbindBuffComponent();
	BoundBuffs = NewBuffs;
	if (NewBuffs) NewBuffs->OnBuffsChanged.AddUniqueDynamic(this, &ThisClass::HandleBuffsChanged);
	RefreshBuffs();
}

void UCC_CharacterScreenController::UnbindBuffComponent()
{
	if (UCC_BuffComponent* Buffs = BoundBuffs.Get()) Buffs->OnBuffsChanged.RemoveDynamic(this, &ThisClass::HandleBuffsChanged);
	BoundBuffs.Reset();
}

void UCC_CharacterScreenController::UnbindPawn()
{
	if (BoundCharacter.IsValid()) BoundCharacter->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleASCReady);
	BoundCharacter.Reset();
	UnbindBuffComponent();
	UnbindASC();
}

// ---------------------------------------------------------------------------
// 属性
// ---------------------------------------------------------------------------

void UCC_CharacterScreenController::HandleAttributeChanged(const FOnAttributeChangeData& Data) { RefreshStats(); }

namespace
{
	FText FormatStat(float Value, ECC_StatDisplayFormat Format)
	{
		FNumberFormattingOptions Options;
		switch (Format)
		{
		case ECC_StatDisplayFormat::Percent:
			Options.SetMaximumFractionalDigits(1);
			return FText::AsPercent(Value, &Options);
		case ECC_StatDisplayFormat::Decimal:
			Options.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(2);
			return FText::AsNumber(Value, &Options);
		default:
			Options.SetMaximumFractionalDigits(0);
			return FText::AsNumber(FMath::RoundToFloat(Value), &Options);
		}
	}
}

// 缺少数据源时发布零值，换 Pawn 期间不残留旧角色数值。
void UCC_CharacterScreenController::RefreshStats()
{
	if (!IsActive()) return;
	const UAbilitySystemComponent* ASC = BoundASC.Get();
	const auto Read = [ASC](const FGameplayAttribute& Attribute) { return ASC && Attribute.IsValid() ? ASC->GetNumericAttribute(Attribute) : 0.f; };

	FText Name;
	if (const APlayerController* Player = GetPlayerController())
	{
		if (const APlayerState* State = Player->PlayerState) Name = FText::FromString(State->GetPlayerName());
	}

	FCC_PlayerVitals Vitals;
	Vitals.Health = Read(UCC_AttributeSet::GetHealthAttribute());
	Vitals.MaxHealth = Read(UCC_AttributeSet::GetMaxHealthAttribute());
	Vitals.Mana = Read(UCC_AttributeSet::GetManaAttribute());
	Vitals.MaxMana = Read(UCC_AttributeSet::GetMaxManaAttribute());

	TArray<FCC_CharacterStatEntry> Stats;
	Stats.Reserve(DisplayedStats.Num());
	for (const FCC_CharacterStatConfig& Config : DisplayedStats)
	{
		if (!Config.Attribute.IsValid()) continue;
		FCC_CharacterStatEntry& Entry = Stats.AddDefaulted_GetRef();
		Entry.Attribute = Config.Attribute;
		Entry.Label = Config.Label;
		Entry.Value = Read(Config.Attribute);
		Entry.ValueText = FormatStat(Entry.Value, Config.Format);
	}

	if (UCC_CharacterScreenModel* ScreenModel = Cast<UCC_CharacterScreenModel>(GetModel()))
	{
		ScreenModel->SetStats(Name, Vitals, MoveTemp(Stats));
	}
}

// ---------------------------------------------------------------------------
// Buff
// ---------------------------------------------------------------------------

void UCC_CharacterScreenController::HandleBuffsChanged() { RefreshBuffs(); }

// 与 CC_BuffBarWidget 相同的过滤规则：GE 上没有 UCC_BuffUIData 或 bHiddenInUI 的不显示。
// 列表顺序保持 BuffComponent 的顺序（施加先后）；模型比较后无变化不会通知界面。
void UCC_CharacterScreenController::RefreshBuffs()
{
	if (!IsActive()) return;
	TArray<FCC_CharacterBuffEntry> Result;
	if (const UCC_BuffComponent* Buffs = BoundBuffs.Get())
	{
		for (const FCC_ReplicatedBuffItem& Item : Buffs->GetActiveBuffs())
		{
			const UGameplayEffect* Effect = Item.EffectClass.GetDefaultObject();
			const UCC_BuffUIData* UIData = Effect ? Effect->FindComponent<UCC_BuffUIData>() : nullptr;
			if (!UIData || UIData->bHiddenInUI) continue;

			FCC_CharacterBuffEntry& Entry = Result.AddDefaulted_GetRef();
			Entry.InstanceId = Item.InstanceId;
			Entry.EffectClass = Item.EffectClass;
			Entry.DisplayName = UIData->DisplayName;
			Entry.Description = UIData->Description;
			Entry.Icon = UIData->Icon;
			Entry.StackCount = FMath::Max(1, Item.StackCount);
			Entry.Level = Item.Level;
			Entry.bIsInhibited = Item.bIsInhibited;
			Entry.Duration = Item.Duration;
			Entry.EndServerTime = Item.EndServerTime;
			Entry.BuffTags = Item.BuffTags;
			Entry.bIsDebuff = DebuffTag.IsValid() && Item.BuffTags.HasTag(DebuffTag);
		}
	}
	if (UCC_CharacterScreenModel* ScreenModel = Cast<UCC_CharacterScreenModel>(GetModel()))
	{
		ScreenModel->SetBuffs(MoveTemp(Result));
	}
}

#undef LOCTEXT_NAMESPACE
