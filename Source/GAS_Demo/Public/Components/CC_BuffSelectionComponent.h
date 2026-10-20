#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "Components/CC_BuffSelectionTypes.h"
#include "CC_BuffSelectionComponent.generated.h"

class APlayerController;
class UCC_ActivatableWidget;
class UCC_BuffOfferPool;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCC_BuffOfferChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCC_BuffChoiceResolved, int32, OfferId, ECC_BuffChoiceResult, Result);

/**
 * 每个玩家一份的 Buff 三选一流程，挂在 PlayerController 上（重生换 Pawn 不影响进行中的一轮）。
 *
 * 服务器：OfferBuffs 抽取选项并排队，同一时间只展示一轮；收到选择后校验编号和下标，
 *         再通过当前 Pawn 的 UCC_BuffComponent 施加 GE，成功才进入下一轮。
 * 客户端：只收到自己的当前一轮（COND_OwnerOnly），RequestChoose 把选择发给服务器；
 *         本地玩家收到新一轮时自动打开配置好的界面。
 *
 * 组件不认识任何具体 Buff，也不修改 GE 配置；施加规则（叠层、免疫等）全部交给 GAS。
 */
UCLASS(ClassGroup = (GAS), meta = (BlueprintSpawnableComponent))
class GAS_DEMO_API UCC_BuffSelectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCC_BuffSelectionComponent();

    /** 查找玩家控制器上的选择组件；没有挂载时返回空。 */
    UFUNCTION(BlueprintPure, Category = "Buff|Selection", meta = (DefaultToSelf = "Player"))
    static UCC_BuffSelectionComponent* FindForPlayer(const APlayerController* Player);

    /**
     * 从池中随机抽一轮给这个玩家；已有一轮未选完时排在后面。
     * @param Pool 本轮使用的 Buff 池。
     * @return 抽到至少一个选项并成功展示或排队时返回 true。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff|Selection")
    bool OfferBuffs(const UCC_BuffOfferPool* Pool);

    /**
     * 直接给出指定的选项（自定义抽取规则时使用）；无效选项会被丢弃。
     * @return 至少有一个有效选项并成功展示或排队时返回 true。
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff|Selection")
    bool OfferOptions(const TArray<FCC_BuffOfferOption>& Options);

    /** 清空当前一轮和排队中的所有轮次，界面随之关闭；不撤销已经施加的 Buff。 */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Buff|Selection")
    void ClearOffers();

    /** 当前等待选择的一轮；没有时 OfferId 为 0。客户端读到的是最近一次复制的结果。 */
    UFUNCTION(BlueprintPure, Category = "Buff|Selection")
    FCC_BuffOffer GetCurrentOffer() const { return CurrentOffer; }

    /** 是否有一轮正在等待选择。 */
    UFUNCTION(BlueprintPure, Category = "Buff|Selection")
    bool HasPendingOffer() const { return CurrentOffer.IsValid(); }

    /** 服务器上排在当前一轮之后的轮数；客户端始终为 0。 */
    UFUNCTION(BlueprintPure, Category = "Buff|Selection")
    int32 GetQueuedOfferCount() const { return QueuedOffers.Num(); }

    /**
     * 本地玩家提交选择；结果通过 OnChoiceResolved 返回，成功时当前一轮随之结束。
     * @param OfferId 界面展示的那一轮编号，用于丢弃针对旧一轮的点击。
     * @param OptionIndex 选中的选项下标。
     * @return 请求已发出时返回 true；编号不符、下标越界或上一次请求还没有结果时返回 false。
     */
    UFUNCTION(BlueprintCallable, Category = "Buff|Selection")
    bool RequestChoose(int32 OfferId, int32 OptionIndex);

    /** 已提交选择、正在等待服务器结果；界面据此禁用卡片，防止连点。 */
    UFUNCTION(BlueprintPure, Category = "Buff|Selection")
    bool IsChoiceInFlight() const { return bChoiceInFlight; }

    /** 当前一轮变化（新的一轮、选完、被清空）时在拥有者客户端广播；服务器本地玩家也会广播。 */
    UPROPERTY(BlueprintAssignable, Category = "Buff|Selection")
    FCC_BuffOfferChanged OnOfferChanged;

    /** 本地请求得到服务器结果时广播；失败原因用于界面提示。 */
    UPROPERTY(BlueprintAssignable, Category = "Buff|Selection")
    FCC_BuffChoiceResolved OnChoiceResolved;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * 把选中的选项施加到玩家当前角色上；测试可覆盖以替换施加目标。
     * @return Accepted 表示 GAS 已接受；NoTarget / ApplyFailed 时本轮保留。
     */
    virtual ECC_BuffChoiceResult ApplyOption(const FCC_BuffOfferOption& Option);

    /** 服务器端的完整校验与施加流程，RPC 和测试共用。 */
    ECC_BuffChoiceResult ResolveChoice(int32 OfferId, int32 OptionIndex);

    /** 本地玩家收到新一轮时自动打开的界面；为空则不自动打开（可自己监听 OnOfferChanged）。 */
    UPROPERTY(EditDefaultsOnly, Category = "Buff|Selection|UI")
    TSubclassOf<UCC_ActivatableWidget> SelectionScreenClass;

    /** 界面打开在哪一层；默认 GameMenu，单机时打开会暂停世界。 */
    UPROPERTY(EditDefaultsOnly, Category = "Buff|Selection|UI", meta = (Categories = "UI.Layer"))
    FGameplayTag ScreenLayer;

private:
    UFUNCTION(Server, Reliable)
    void ServerChoose(int32 OfferId, int32 OptionIndex);

    UFUNCTION(Client, Reliable)
    void ClientChoiceResolved(int32 OfferId, ECC_BuffChoiceResult Result);

    UFUNCTION()
    void OnRep_CurrentOffer();

    /** 服务器入口：分配编号，空闲时直接展示，否则排队。 */
    bool EnqueueOffer(TArray<FCC_BuffOfferOption>&& Options);
    /** 服务器写入当前一轮并立即同步；本地玩家同步走 OnRep 之外的通知。 */
    void SetCurrentOffer(FCC_BuffOffer&& Offer);
    /** 当前一轮结束后展示下一轮，没有则清空。 */
    void AdvanceQueue();
    /** 当前一轮变化后的本地处理：结束请求状态、广播、开关界面。 */
    void HandleOfferChangedLocally();
    APlayerController* GetOwningPlayer() const;
    bool IsLocalPlayer() const;

    /** 尝试打开界面，根布局未就绪或正在过渡时启动重试。 */
    void TryOpenScreen();
    /** 打开一次；界面已在栈中也视为成功，不重复入栈。 */
    bool OpenScreenOnce() const;
    void StopOpenRetry();

    /** 只复制给拥有者：其他玩家看不到你的选项。 */
    UPROPERTY(ReplicatedUsing = OnRep_CurrentOffer)
    FCC_BuffOffer CurrentOffer;

    /** 服务器上排队等待展示的轮次，不复制。 */
    UPROPERTY(Transient)
    TArray<FCC_BuffOffer> QueuedOffers;

    /** 服务器随机流；BeginPlay 时随机播种。 */
    FRandomStream Random;
    int32 NextOfferId = 1;

    /** 客户端请求状态：等待结果期间禁止再次提交。 */
    bool bChoiceInFlight = false;
    int32 InFlightOfferId = 0;

    /** 不受世界暂停影响的重试计时，用于根布局尚未就绪或正在过渡时打开界面。 */
    FTSTicker::FDelegateHandle OpenRetryHandle;
};
