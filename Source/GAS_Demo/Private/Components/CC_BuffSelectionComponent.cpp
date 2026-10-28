#include "Components/CC_BuffSelectionComponent.h"
#include "Data/Buff/CC_BuffOfferPool.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Components/CC_BuffComponent.h"
#include "Data/Buff/CC_BuffDebug.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTags/CC_Tags.h"
#include "Net/UnrealNetwork.h"
#include "UI/Framework/CC_ActivatableWidget.h"
#include "UI/Framework/CC_RootLayout.h"
#include "UI/Framework/CC_UIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

UCC_BuffSelectionComponent::UCC_BuffSelectionComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    ScreenLayer = CCTags::UILayer::GameMenu;
}

UCC_BuffSelectionComponent* UCC_BuffSelectionComponent::FindForPlayer(const APlayerController* Player)
{
    return Player ? Player->FindComponentByClass<UCC_BuffSelectionComponent>() : nullptr;
}

void UCC_BuffSelectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UCC_BuffSelectionComponent, CurrentOffer, COND_OwnerOnly);
}

void UCC_BuffSelectionComponent::BeginPlay()
{
    Super::BeginPlay();
    Random.Initialize(FMath::Rand());
    if (!Cast<APlayerController>(GetOwner()))
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[Selection] %s 应挂在 PlayerController 上，当前所有者为 %s"), *GetName(),
               *GetNameSafe(GetOwner()));
    }
    // 客户端可能在 BeginPlay 之前就收到了当前一轮。
    if (HasPendingOffer() && IsLocalPlayer())
    {
        HandleOfferChangedLocally();
    }
}

void UCC_BuffSelectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopOpenRetry();
    Super::EndPlay(EndPlayReason);
}

APlayerController* UCC_BuffSelectionComponent::GetOwningPlayer() const
{
    return Cast<APlayerController>(GetOwner());
}

bool UCC_BuffSelectionComponent::IsLocalPlayer() const
{
    const APlayerController* Player = GetOwningPlayer();
    return Player && Player->IsLocalController();
}

// ---------------------------------------------------------------------------
// 服务器：发起与排队
// ---------------------------------------------------------------------------

bool UCC_BuffSelectionComponent::OfferBuffs(const UCC_BuffOfferPool* Pool)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Pool)
    {
        return false;
    }
    TArray<FCC_BuffOfferOption> Options = Pool->RollOptions(Random);
    if (Options.IsEmpty())
    {
        UE_LOG(LogCCBuff, Warning, TEXT("[Selection] 池 %s 没有可抽取的有效条目"), *GetNameSafe(Pool));
        return false;
    }
    return EnqueueOffer(MoveTemp(Options));
}

bool UCC_BuffSelectionComponent::OfferOptions(const TArray<FCC_BuffOfferOption>& Options)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return false;
    }
    TArray<FCC_BuffOfferOption> Usable;
    for (const FCC_BuffOfferOption& Option : Options)
    {
        if (UCC_BuffOfferPool::IsOptionUsable(Option))
        {
            Usable.Add(Option);
        }
    }
    return !Usable.IsEmpty() && EnqueueOffer(MoveTemp(Usable));
}

bool UCC_BuffSelectionComponent::EnqueueOffer(TArray<FCC_BuffOfferOption>&& Options)
{
    FCC_BuffOffer Offer;
    Offer.OfferId = NextOfferId++;
    Offer.Options = MoveTemp(Options);
    UE_LOG(LogCCBuff, Log, TEXT("[Selection] Offer %d -> %s，%d 个选项%s"), Offer.OfferId,
           *GetNameSafe(GetOwner()), Offer.Options.Num(), HasPendingOffer() ? TEXT("（排队）") : TEXT(""));
    if (HasPendingOffer())
    {
        QueuedOffers.Add(MoveTemp(Offer));
        CurrentOffer.PendingCount = QueuedOffers.Num() + 1;
        GetOwner()->ForceNetUpdate();
        if (IsLocalPlayer()) HandleOfferChangedLocally();
    }
    else
    {
        SetCurrentOffer(MoveTemp(Offer));
    }
    return true;
}

void UCC_BuffSelectionComponent::ClearOffers()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }
    QueuedOffers.Reset();
    if (HasPendingOffer())
    {
        SetCurrentOffer(FCC_BuffOffer());
    }
}

void UCC_BuffSelectionComponent::SetCurrentOffer(FCC_BuffOffer&& Offer)
{
    const bool bWasPending = HasPendingOffer();
    CurrentOffer = MoveTemp(Offer);
    CurrentOffer.PendingCount = HasPendingOffer() ? QueuedOffers.Num() + 1 : 0;
    if (!HasPendingOffer()) ServerSetSelectionActive_Implementation(false);
    GetOwner()->ForceNetUpdate();
    if (bWasPending != HasPendingOffer())
    {
        // 只有服务器会调用 SetCurrentOffer（客户端走 OnRep），这里即服务器广播。
        OnServerPendingChanged.Broadcast(HasPendingOffer());
    }
    // 服务器不会收到自己的 OnRep；监听服务器主机与单机玩家在这里处理。
    if (IsLocalPlayer())
    {
        HandleOfferChangedLocally();
    }
}

void UCC_BuffSelectionComponent::AdvanceQueue()
{
    if (QueuedOffers.IsEmpty())
    {
        SetCurrentOffer(FCC_BuffOffer());
        return;
    }
    FCC_BuffOffer Next = MoveTemp(QueuedOffers[0]);
    QueuedOffers.RemoveAt(0);
    SetCurrentOffer(MoveTemp(Next));
}

// ---------------------------------------------------------------------------
// 选择：客户端请求 → 服务器校验与施加 → 结果回传
// ---------------------------------------------------------------------------

bool UCC_BuffSelectionComponent::RequestChoose(int32 OfferId, int32 OptionIndex)
{
    if (bChoiceInFlight || !HasPendingOffer() || OfferId != CurrentOffer.OfferId ||
        !CurrentOffer.Options.IsValidIndex(OptionIndex))
    {
        return false;
    }
    bChoiceInFlight = true;
    InFlightOfferId = OfferId;
    // 单机与监听服务器主机会在这里同步执行完整流程。
    ServerChoose(OfferId, OptionIndex);
    return true;
}

void UCC_BuffSelectionComponent::ServerChoose_Implementation(int32 OfferId, int32 OptionIndex)
{
    const ECC_BuffChoiceResult Result = ResolveChoice(OfferId, OptionIndex);
    ClientChoiceResolved(OfferId, Result);
}

ECC_BuffChoiceResult UCC_BuffSelectionComponent::ResolveChoice(int32 OfferId, int32 OptionIndex)
{
    if (!HasPendingOffer() || OfferId != CurrentOffer.OfferId)
    {
        return ECC_BuffChoiceResult::StaleOffer;
    }
    if (!CurrentOffer.Options.IsValidIndex(OptionIndex))
    {
        return ECC_BuffChoiceResult::InvalidIndex;
    }
    const FCC_BuffOfferOption Option = CurrentOffer.Options[OptionIndex];
    const ECC_BuffChoiceResult Result = ApplyOption(Option);
    UE_LOG(LogCCBuff, Log, TEXT("[Selection] Offer %d 选择 %d (%s) -> %s"), OfferId, OptionIndex,
           *GetNameSafe(Option.Effect.Get()), *UEnum::GetValueAsString(Result));
    if (Result == ECC_BuffChoiceResult::Accepted)
    {
        OnServerChoiceAccepted.Broadcast(Option);
        AdvanceQueue();
    }
    return Result;
}

ECC_BuffChoiceResult UCC_BuffSelectionComponent::ApplyOption(const FCC_BuffOfferOption& Option)
{
    const APlayerController* Player = GetOwningPlayer();
    ACC_BaseCharacter* Character = Player ? Cast<ACC_BaseCharacter>(Player->GetPawn()) : nullptr;
    UCC_BuffComponent* Buffs = Character ? Character->GetBuffComponent() : nullptr;
    if (!Buffs || !Character->IsAlive() || Character->IsActorBeingDestroyed())
    {
        return ECC_BuffChoiceResult::NoTarget;
    }
    // Instant GE 成功时没有活动句柄，必须用 WasSuccessfullyApplied 判断。
    return Buffs->ApplyBuff(Option.Effect, nullptr, Option.Level).WasSuccessfullyApplied()
               ? ECC_BuffChoiceResult::Accepted
               : ECC_BuffChoiceResult::ApplyFailed;
}

void UCC_BuffSelectionComponent::ClientChoiceResolved_Implementation(int32 OfferId, ECC_BuffChoiceResult Result)
{
    // 成功时等当前一轮的复制更新再解除锁定，避免结果先到、旧卡片短暂可点。
    if (Result != ECC_BuffChoiceResult::Accepted && OfferId == InFlightOfferId)
    {
        bChoiceInFlight = false;
    }
    OnChoiceResolved.Broadcast(OfferId, Result);
}

// ---------------------------------------------------------------------------
// 本地：广播与界面
// ---------------------------------------------------------------------------

void UCC_BuffSelectionComponent::OnRep_CurrentOffer()
{
    HandleOfferChangedLocally();
}

void UCC_BuffSelectionComponent::HandleOfferChangedLocally()
{
    // 当前一轮换了（选完、清空或进入下一轮），之前的请求自然结束。
    if (CurrentOffer.OfferId != InFlightOfferId)
    {
        bChoiceInFlight = false;
    }
    OnOfferChanged.Broadcast();
    if (!HasPendingOffer())
    {
        // 界面在没有选项后自行关闭，这里只停止尚未完成的打开重试。
        StopOpenRetry();
    }
}

bool UCC_BuffSelectionComponent::RequestOpenSelection()
{
    if (!IsLocalPlayer() || !HasPendingOffer() || !SelectionScreenClass) return false;
    TryOpenScreen();
    return true;
}

void UCC_BuffSelectionComponent::ServerSetSelectionActive_Implementation(bool bActive)
{
    const bool bNewActive = bActive && HasPendingOffer();
    if (bSelectionActive == bNewActive) return;
    bSelectionActive = bNewActive;
    OnServerSelectionActiveChanged.Broadcast(bSelectionActive);
}

void UCC_BuffSelectionComponent::TryOpenScreen()
{
    if (!SelectionScreenClass || OpenRetryHandle.IsValid() || OpenScreenOnce())
    {
        return;
    }
    // 根布局可能尚未创建或正在播放过渡；用核心 Ticker 重试，世界暂停时也会继续。
    OpenRetryHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateWeakLambda(this,
                                          [this](float)
                                          {
                                              if (!HasPendingOffer() || OpenScreenOnce())
                                              {
                                                  OpenRetryHandle.Reset();
                                                  return false;
                                              }
                                              return true;
                                          }),
        0.1f);
}

bool UCC_BuffSelectionComponent::OpenScreenOnce() const
{
    UCC_RootLayout* Root = UCC_UIManagerSubsystem::GetRootLayoutForPlayer(GetOwningPlayer());
    if (!Root || !SelectionScreenClass)
    {
        return false;
    }
    // 已在栈中时不再调用 ShowScreen，避免重复触发 On Screen Opened；界面自己监听本组件刷新内容。
    // 正在退场的旧页面仍留在栈里但已失活：等它释放后再打开新的，否则 ShowScreen 会返回这个退场中的实例。
    if (const UCommonActivatableWidgetContainerBase* Stack = Root->GetLayer(ScreenLayer))
    {
        for (const UCommonActivatableWidget* Existing : Stack->GetWidgetList())
        {
            if (Existing && Existing->GetClass() == SelectionScreenClass.Get())
            {
                return Existing->IsActivated();
            }
        }
    }
    return Root->ShowScreen(ScreenLayer, SelectionScreenClass, nullptr) != nullptr;
}

void UCC_BuffSelectionComponent::StopOpenRetry()
{
    if (OpenRetryHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(OpenRetryHandle);
        OpenRetryHandle.Reset();
    }
}
