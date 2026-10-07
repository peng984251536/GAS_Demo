#include "UI/DamageText/CC_DamageTextController.h"
#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UCC_DamageTextController::UCC_DamageTextController() { ModelClass = UCC_DamageTextModel::StaticClass(); }

// 飘字子系统按世界存在，取玩家所在世界而不是缓存的旧世界。
void UCC_DamageTextController::OnActivated()
{
	APlayerController* Player = GetPlayerController();
	UWorld* World = Player ? Player->GetWorld() : nullptr;
	if (UCC_DamageTextSubsystem* Subsystem = World ? World->GetSubsystem<UCC_DamageTextSubsystem>() : nullptr)
	{
		BoundSubsystem = Subsystem;
		RegisteredPlayer = Player;
		UpdatedHandle = Subsystem->OnEntriesUpdated.AddUObject(this, &ThisClass::Refresh);
		WarnIfDuplicateLayer(Subsystem->AddDrawLayer(Player), TEXT("伤害飘字"),
			TEXT("根布局已自动创建飘字层，请删除 HUD 蓝图里旧的 Damage Text Widget。"));
	}
	Refresh();
	Super::OnActivated();
}

// 只撤销本控制器的订阅和登记；子系统可能已随世界销毁，弱引用为空时跳过。
void UCC_DamageTextController::OnDeactivated()
{
	if (UCC_DamageTextSubsystem* Subsystem = BoundSubsystem.Get())
	{
		Subsystem->OnEntriesUpdated.Remove(UpdatedHandle);
		Subsystem->RemoveDrawLayer(RegisteredPlayer.Get());
	}
	UpdatedHandle.Reset();
	BoundSubsystem.Reset();
	RegisteredPlayer.Reset();
	if (UCC_DamageTextModel* TextModel = Cast<UCC_DamageTextModel>(GetModel())) TextModel->Entries.Reset();
	Super::OnDeactivated();
}

// 活动飘字通常只有十几个，整份复制的成本可以忽略，换来视图与子系统完全解耦。
void UCC_DamageTextController::Refresh()
{
	UCC_DamageTextModel* TextModel = Cast<UCC_DamageTextModel>(GetModel());
	if (!TextModel) return;
	const UCC_DamageTextSubsystem* Subsystem = BoundSubsystem.Get();
	if (!IsActive() || !Subsystem)
	{
		TextModel->Entries.Reset();
		return;
	}
	TextModel->Entries = Subsystem->GetActiveEntries();
	TextModel->Lifetime = FMath::Max(Subsystem->Lifetime, KINDA_SMALL_NUMBER);
}
