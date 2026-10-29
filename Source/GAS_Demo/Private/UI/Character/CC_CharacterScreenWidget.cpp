#include "UI/Character/CC_CharacterScreenWidget.h"
#include "UI/Character/CC_CharacterScreenController.h"
#include "UI/Character/CC_CharacterBuffEntryWidget.h"
#include "Components/PanelWidget.h"

UCC_CharacterScreenWidget::UCC_CharacterScreenWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ControllerClass = UCC_CharacterScreenController::StaticClass();
	InputMode = ECommonInputMode::Menu;
	bAllowBack = true;
}

float UCC_CharacterScreenWidget::GetBuffRemainingTime(const FCC_CharacterBuffEntry& Buff) const
{
	return CCCharacterScreen::GetRemainingTime(GetWorld(), Buff);
}

void UCC_CharacterScreenWidget::NativeOnActivated()
{
	// 必须在 Super 之前：Super 会激活控制器，控制器绑定数据源时就会推送模型。
	SeenStatsRevision = INDEX_NONE;
	SeenBuffsRevision = INDEX_NONE;
	Super::NativeOnActivated();
}

void UCC_CharacterScreenWidget::NativeOnModelChanged()
{
	if (const UCC_CharacterScreenModel* ScreenModel = Cast<UCC_CharacterScreenModel>(GetUIModel()))
	{
		if (ScreenModel->GetStatsRevision() != SeenStatsRevision)
		{
			SeenStatsRevision = ScreenModel->GetStatsRevision();
			OnStatsChanged(ScreenModel->GetDisplayName(), ScreenModel->GetVitals(), ScreenModel->GetStats());
		}
		if (ScreenModel->GetBuffsRevision() != SeenBuffsRevision)
		{
			SeenBuffsRevision = ScreenModel->GetBuffsRevision();
			SyncBuffList(ScreenModel->GetBuffs());
			OnBuffsChanged(ScreenModel->GetBuffs());
		}
	}
	Super::NativeOnModelChanged();
}

// 叠层、刷新时长只更新原条目，不重建；Buff 增删时才重排容器。
void UCC_CharacterScreenWidget::SyncBuffList(const TArray<FCC_CharacterBuffEntry>& Buffs)
{
	if (!BuffList || !BuffEntryClass) return;

	// 条目集合和顺序都没变（叠层、刷新时长、抑制）：原地更新，不动容器。
	bool bSameOrder = EntriesById.Num() == Buffs.Num() && BuffList->GetChildrenCount() == Buffs.Num();
	for (int32 Index = 0; bSameOrder && Index < Buffs.Num(); ++Index)
	{
		const TObjectPtr<UCC_CharacterBuffEntryWidget>* Found = EntriesById.Find(Buffs[Index].InstanceId);
		bSameOrder = Found && BuffList->GetChildAt(Index) == Found->Get();
	}
	if (bSameOrder)
	{
		for (const FCC_CharacterBuffEntry& Buff : Buffs) EntriesById[Buff.InstanceId]->SetBuff(Buff);
		return;
	}

	// 有增删：复用仍存在的条目对象，按新顺序重新放入容器。
	TMap<int32, TObjectPtr<UCC_CharacterBuffEntryWidget>> Previous = MoveTemp(EntriesById);
	EntriesById.Reset();
	BuffList->ClearChildren();
	for (const FCC_CharacterBuffEntry& Buff : Buffs)
	{
		UCC_CharacterBuffEntryWidget* Entry = nullptr;
		if (TObjectPtr<UCC_CharacterBuffEntryWidget>* Found = Previous.Find(Buff.InstanceId)) Entry = Found->Get();
		if (!Entry) Entry = CreateWidget<UCC_CharacterBuffEntryWidget>(this, BuffEntryClass);
		if (!Entry) continue;
		Entry->SetBuff(Buff);
		BuffList->AddChild(Entry);
		EntriesById.Add(Buff.InstanceId, Entry);
	}
}
