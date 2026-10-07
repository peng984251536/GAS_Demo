#include "UI/HealthBar/CC_HealthBarOverlayController.h"
#include "UI/HealthBar/CC_BatchedHealthBarSubsystem.h"
#include "Components/Widget.h"
#include "GameFramework/Actor.h"
#include "GAS_Demo.h"

UCC_HealthBarOverlayController::UCC_HealthBarOverlayController() { ModelClass = UCC_HealthBarOverlayModel::StaticClass(); }

// 先订阅再推送初值，保证激活后第一帧就有快照。
void UCC_HealthBarOverlayController::OnActivated()
{
	if (UCC_BatchedHealthBarSubsystem* Manager = UCC_BatchedHealthBarSubsystem::GetHealthBarManager(GetPlayerController()))
	{
		BoundManager = Manager;
		UpdatedHandle = Manager->OnEntriesUpdated.AddUObject(this, &ThisClass::Refresh);
		if (Manager->AddDrawLayer() > 1)
		{
			UE_LOG(LogGAS_Demo, Warning, TEXT("同一本地玩家存在多个头顶血条绘制层（%s），血条会被重复绘制。")
				TEXT("根布局已自动创建血条层，请删除 PlayerController 等处手动 Add to Player Screen 的血条控件。"), *GetPathNameSafe(View.Get()));
		}
	}
	Refresh();
	Super::OnActivated();
}

// 精确移除本控制器的订阅和登记，不影响同一子系统的其他观察者。
void UCC_HealthBarOverlayController::OnDeactivated()
{
	if (UCC_BatchedHealthBarSubsystem* Manager = BoundManager.Get())
	{
		Manager->OnEntriesUpdated.Remove(UpdatedHandle);
		Manager->RemoveDrawLayer();
	}
	UpdatedHandle.Reset();
	BoundManager.Reset();
	if (UCC_HealthBarOverlayModel* OverlayModel = Cast<UCC_HealthBarOverlayModel>(GetModel())) OverlayModel->Items.Reset();
	Super::OnDeactivated();
}

// 显示规则集中在这里，视图只负责投影、距离剔除和绘制。
void UCC_HealthBarOverlayController::Refresh()
{
	UCC_HealthBarOverlayModel* OverlayModel = Cast<UCC_HealthBarOverlayModel>(GetModel());
	if (!OverlayModel) return;
	OverlayModel->Items.Reset();
	const UCC_BatchedHealthBarSubsystem* Manager = BoundManager.Get();
	if (!IsActive() || !Manager) return;
	for (const FCC_HealthBarEntry& Entry : Manager->GetEntries())
	{
		const AActor* Actor = Entry.Actor.Get();
		if (!Actor || !Entry.bVisible || !Entry.bReady || Actor->IsHidden() || Entry.MaxHealth <= 0) continue;
		if (Entry.Options.bHideWhenDead && Entry.Health <= 0) continue;
		if (Entry.Options.bHideWhenFull && Entry.Health >= Entry.MaxHealth) continue;
		FCC_HealthBarDisplayItem& Item = OverlayModel->Items.AddDefaulted_GetRef();
		Item.WorldAnchor = Actor->GetActorLocation() + Entry.Options.WorldOffset;
		Item.Size = Entry.Options.Size;
		Item.Color = Entry.Options.Color;
		Item.Fraction = FMath::Clamp(Entry.DisplayedFraction, 0.f, 1.f);
		Item.MaxDistance = Entry.Options.MaxDistance;
	}
}
