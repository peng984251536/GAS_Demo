#include "UI/DamageText/CC_DamageTextWidget.h"
#include "UI/DamageText/CC_DamageTextEntryWidget.h"
#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "UI/DamageText/SCC_DamageTextLayer.h"
#include "UI/WorldOverlay/CC_WorldOverlayProjection.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GAS_Demo.h"

/**
 * 配置默认模板，一个类通过样式数据显示所有数字。
 * @param ObjectInitializer 引擎提供的对象初始化参数。
 */
UCC_DamageTextWidget::UCC_DamageTextWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
    EntryWidgetClass = UCC_DamageTextEntryWidget::StaticClass();
}

/** 清空池和 Slate 子槽；先移出成员表，再释放，避免蓝图析构回调重入表迭代。 */
void UCC_DamageTextWidget::ResetWidgets()
{
    if (bResettingWidgets) return;
    TGuardValue<bool> Guard(bResettingWidgets, true);
    auto RetiredActive = MoveTemp(ActiveWidgets);
    auto RetiredPool = MoveTemp(PooledWidgets);
    ActiveWidgets.Empty();
    PooledWidgets.Empty();
    if (DamageTextLayer.IsValid()) DamageTextLayer->ClearChildren();
    for (auto& Pair : RetiredActive)
    {
        if (IsValid(Pair.Value.Widget)) { Pair.Value.Widget->StopAllAnimations(); Pair.Value.Widget->ReleaseSlateResources(true); }
        Pair.Value.SlateWidget.Reset();
    }
    for (auto& State : RetiredPool)
    {
        if (IsValid(State.Widget)) { State.Widget->StopAllAnimations(); State.Widget->ReleaseSlateResources(true); }
        State.SlateWidget.Reset();
    }
}

/**
 * 统一换绑与解绑；池内 Widget 的 Owning Player 不能跨世界沿用。
 * @param Source 新的飘字子系统，nullptr 表示解绑。
 * @param Player 当前本地玩家，解绑时为空。
 */
void UCC_DamageTextWidget::SetSource(UCC_DamageTextSubsystem* Source, APlayerController* Player)
{
    if (BoundSubsystem.Get() == Source && RegisteredPlayer.Get() == Player) return;
    if (auto* Previous = BoundSubsystem.Get())
    {
        Previous->OnEntriesUpdated.Remove(UpdatedHandle);
        Previous->RemoveDrawLayer(RegisteredPlayer.Get());
    }
    UpdatedHandle.Reset();
    BoundSubsystem.Reset();
    RegisteredPlayer.Reset();
    ResetWidgets();
    BoundSubsystem = Source;
    RegisteredPlayer = Player;
    if (!Source || !Player) return;
    if (Source->AddDrawLayer(Player) > 1)
        UE_LOG(LogGAS_Demo, Warning, TEXT("同一本地玩家有多个伤害数字层（%s）；请移除重复 HUD 控件。"), *GetPathName());
    UpdatedHandle = Source->OnEntriesUpdated.AddWeakLambda(this, [this]() { SyncWidgets(); });
    SyncWidgets();
}

/** 同步控件实例；临时快照只用于防止蓝图回调修改源列表，不持久建立第二份业务 Model。 */
void UCC_DamageTextWidget::SyncWidgets()
{
    auto* Source = BoundSubsystem.Get();
    auto* Player = RegisteredPlayer.Get();
    if (bSyncingWidgets || bResettingWidgets || !DamageTextLayer.IsValid() || !Source || !Player
        || GetOwningPlayer() != Player || Source->GetWorld() != Player->GetWorld()) return;
    TGuardValue<bool> Guard(bSyncingWidgets, true);
    UClass* DesiredClass = EntryWidgetClass.Get();
    if (!DesiredClass || DesiredClass->HasAnyClassFlags(CLASS_Abstract)) DesiredClass = UCC_DamageTextEntryWidget::StaticClass();
    if (CachedEntryClass.Get() != DesiredClass)
    {
        ResetWidgets();
        CachedEntryClass = DesiredClass;
    }
    const TArray<FCC_DamageTextEntry> Entries = Source->GetActiveEntries();
    TSet<uint64> LiveIds;
    for (const auto& Entry : Entries) if (Entry.Viewer == Player) LiveIds.Add(Entry.Id);
    TArray<uint64> RetiredIds;
    for (const auto& Pair : ActiveWidgets) if (!LiveIds.Contains(Pair.Key)) RetiredIds.Add(Pair.Key);
    for (uint64 Id : RetiredIds)
    {
        FCC_DamageTextWidgetState State;
        if (!ActiveWidgets.RemoveAndCopyValue(Id, State)) continue;
        if (State.SlateWidget.IsValid()) DamageTextLayer->RemoveSlot(State.SlateWidget.ToSharedRef());
        State.Slot = nullptr;
        State.bInitialized = false;
        if (IsValid(State.Widget))
        {
            State.Widget->StopAllAnimations();
            State.Widget->SetRenderOpacity(0.f);
            State.Widget->SetRenderScale(FVector2D::UnitVector);
            if (PooledWidgets.Num() < FMath::Max(0, MaxPooledWidgets)) PooledWidgets.Add(MoveTemp(State));
            else State.Widget->ReleaseSlateResources(true);
        }
        if (!DamageTextLayer.IsValid() || BoundSubsystem.Get() != Source || RegisteredPlayer.Get() != Player) return;
    }
    for (const auto& Entry : Entries)
    {
        if (Entry.Viewer != Player) continue;
        if (!ActiveWidgets.Contains(Entry.Id))
        {
            FCC_DamageTextWidgetState NewState;
            if (!PooledWidgets.IsEmpty()) NewState = PooledWidgets.Pop(EAllowShrinking::No);
            else
            {
                NewState.Widget = CreateWidget<UCC_DamageTextEntryWidget>(Player, DesiredClass);
                if (!NewState.Widget) continue;
                NewState.SlateWidget = NewState.Widget->TakeWidget();
            }
            if (!DamageTextLayer.IsValid() || BoundSubsystem.Get() != Source || RegisteredPlayer.Get() != Player) return;
            if (!NewState.SlateWidget.IsValid()) NewState.SlateWidget = NewState.Widget->TakeWidget();
            NewState.SlateWidget->SetVisibility(EVisibility::Hidden); // 首次投影前不显示在左上角。
            auto& Stored = ActiveWidgets.Add(Entry.Id, MoveTemp(NewState));
            DamageTextLayer->AddSlot().Expose(Stored.Slot).Anchors(FAnchors(0.f, 0.f))
                .Alignment(FVector2D(0.5f, 0.5f)).AutoSize(true).ZOrder(0.f)[Stored.SlateWidget.ToSharedRef()];
        }
        auto& State = ActiveWidgets.FindChecked(Entry.Id);
        const float Size = Entry.Style == ECC_DamageTextStyle::Critical ? CriticalFontSize : FontSize;
        const int32 DesiredSize = FMath::RoundToInt(FMath::IsFinite(Size) ? FMath::Clamp(Size, 1.f, 512.f) : 28.f);
        if (!State.bInitialized || State.LastAmount != Entry.Amount || State.LastStyle != Entry.Style
            || State.LastType != Entry.DamageType || State.LastFontSize != DesiredSize)
        {
            State.bInitialized = true;
            State.LastAmount = Entry.Amount;
            State.LastStyle = Entry.Style;
            State.LastType = Entry.DamageType;
            State.LastFontSize = DesiredSize;
            // 缓存先更新，蓝图事件之后不再使用 State 引用，允许源数据回调重入。
            State.Widget->UpdateDamageText(Entry.Amount, Entry.Style, Entry.DamageType, DesiredSize);
            if (!DamageTextLayer.IsValid() || BoundSubsystem.Get() != Source || RegisteredPlayer.Get() != Player) return;
        }
    }
}

/**
 * 统一更新位置和基础动画，不给每个数字写独立 Tick。
 * @param Geometry 当前全屏容器的布局几何。
 */
void UCC_DamageTextWidget::UpdatePositions(const FGeometry& Geometry)
{
    auto* Source = BoundSubsystem.Get();
    auto* Player = RegisteredPlayer.Get();
    if (!Source || !Player || GetOwningPlayer() != Player || Source->GetWorld() != Player->GetWorld() || ActiveWidgets.IsEmpty()) return;
    FCC_WorldOverlayProjector Projector;
    const bool bCanProject = Projector.Initialize(Player, Geometry);
    const float Lifetime = FMath::Max(0.1f, Source->Lifetime);
    for (const auto& Entry : Source->GetActiveEntries())
    {
        if (Entry.Viewer != Player) continue;
        auto* State = ActiveWidgets.Find(Entry.Id);
        if (!State || !State->Widget || !State->Slot || !State->SlateWidget.IsValid()) continue;
        FVector2D Anchor;
        if (!bCanProject || !Projector.ProjectToPlayerLocal(Entry.WorldLocation, Anchor, false))
        {
            State->SlateWidget->SetVisibility(EVisibility::Hidden);
            continue;
        }
        const float T = FMath::Clamp(Entry.Elapsed / Lifetime, 0.f, 1.f);
        const float Alpha = T < 0.15f ? T / 0.15f : FMath::Clamp((1.f - T) / 0.35f, 0.f, 1.f);
        const FVector2D Position = Projector.PlayerLocalToWidget(Anchor
            - FVector2D(Entry.StackOffset.X, Entry.StackOffset.Y + RiseHeight * FMath::Sqrt(T)));
        State->Slot->SetOffset(FMargin(Position.X, Position.Y, 0.f, 0.f));
        State->SlateWidget->SetVisibility(EVisibility::HitTestInvisible);
        State->Widget->SetRenderOpacity(Alpha);
        const float Scale = 1.f + Entry.Pulse * 0.18f;
        State->Widget->SetRenderScale(FVector2D(Scale, Scale));
    }
}

/**
 * 创建唯一公共布局面板，不再直接提交数字 MakeText 绘制元素。
 * @return 公共布局面板。
 */
TSharedRef<SWidget> UCC_DamageTextWidget::RebuildWidget()
{
    ResetWidgets();
    DamageTextLayer = SNew(SCC_DamageTextLayer).OwnerWidget(this);
    APlayerController* Player = IsDesignTime() ? nullptr : GetOwningPlayer();
    UWorld* World = Player && Player->IsLocalController() ? Player->GetWorld() : nullptr;
    SetSource(World ? World->GetSubsystem<UCC_DamageTextSubsystem>() : nullptr, World ? Player : nullptr);
    SyncWidgets();
    return DamageTextLayer.ToSharedRef();
}

/** 刷新数据源和模板设置，样式变化只更新现有 UMG。 */
void UCC_DamageTextWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    APlayerController* Player = IsDesignTime() ? nullptr : GetOwningPlayer();
    UWorld* World = Player && Player->IsLocalController() ? Player->GetWorld() : nullptr;
    SetSource(World ? World->GetSubsystem<UCC_DamageTextSubsystem>() : nullptr, World ? Player : nullptr);
    SyncWidgets();
}

/**
 * 清理旧世界的 UMG、Slate 和委托引用。
 * @param bReleaseChildren 是否同时释放子控件资源。
 */
void UCC_DamageTextWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    SetSource(nullptr, nullptr);
    ResetWidgets();
    DamageTextLayer.Reset();
    Super::ReleaseSlateResources(bReleaseChildren);
}
