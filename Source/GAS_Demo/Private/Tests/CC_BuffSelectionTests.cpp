#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/CC_BuffSelectionTestTypes.h"
#include "Tests/CC_BuffTestTypes.h"
#include "Data/Buff/CC_BuffOfferPool.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

namespace CCBuffSelectionTest
{
    FCC_BuffOfferOption Option(TSubclassOf<UGameplayEffect> Effect, float Level = 1.f)
    {
        FCC_BuffOfferOption Result;
        Result.Effect = Effect;
        Result.Level = Level;
        return Result;
    }

    /** 3 个有效条目，外加重复、空类、非法等级和缺少 UIData 的无效条目。 */
    UCC_BuffOfferPool* MakePool(int32 OfferCount)
    {
        UCC_BuffOfferPool* Pool = NewObject<UCC_BuffOfferPool>();
        Pool->OfferCount = OfferCount;
        Pool->Entries = {
            Option(UCC_BuffTestEffect::StaticClass()),
            Option(UCC_BuffTestOtherEffect::StaticClass(), 2.f),
            Option(UCC_BuffTestInstantEffect::StaticClass()),
            Option(UCC_BuffTestEffect::StaticClass(), 3.f),          // 重复的 GE 类
            Option(nullptr),                                         // 空类
            Option(UCC_BuffTestOtherEffect::StaticClass(), 0.f),     // 非法等级（且类重复）
            Option(UCC_BuffTestPeriodicEffect::StaticClass()),       // 没有 UCC_BuffUIData
        };
        return Pool;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffSelectionRollTest, "GAS_Demo.Buff.Selection.Roll",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 抽取：数量、同轮不重复、跳过无效条目、池不足时全部给出，并且每个有效条目都有机会出现。
 */
bool FCCBuffSelectionRollTest::RunTest(const FString& Parameters)
{
    using namespace CCBuffSelectionTest;
    TStrongObjectPtr<UCC_BuffOfferPool> Pool(MakePool(2));
    TSet<UClass*> Seen;
    for (int32 Seed = 0; Seed < 64; ++Seed)
    {
        FRandomStream Random(Seed);
        const TArray<FCC_BuffOfferOption> Options = Pool->RollOptions(Random);
        if (!TestEqual(TEXT("每轮数量等于 OfferCount"), Options.Num(), 2))
        {
            return false;
        }
        TestTrue(TEXT("同一轮 GE 不重复"), Options[0].Effect != Options[1].Effect);
        for (const FCC_BuffOfferOption& Rolled : Options)
        {
            TestTrue(TEXT("只抽有效条目"), UCC_BuffOfferPool::IsOptionUsable(Rolled));
            TestTrue(TEXT("缺少 UIData 的条目被跳过"),
                     Rolled.Effect.Get() != UCC_BuffTestPeriodicEffect::StaticClass());
            if (Rolled.Effect == UCC_BuffTestEffect::StaticClass())
            {
                TestEqual(TEXT("重复条目保留第一条的等级"), Rolled.Level, 1.f);
            }
            Seen.Add(Rolled.Effect.Get());
        }
    }
    TestEqual(TEXT("三个有效条目都出现过"), Seen.Num(), 3);

    Pool->OfferCount = 5;
    FRandomStream Random(7);
    TestEqual(TEXT("有效条目不足时全部给出"), Pool->RollOptions(Random).Num(), 3);

    Pool->Entries.Reset();
    TestEqual(TEXT("空池返回空"), Pool->RollOptions(Random).Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCBuffSelectionServerFlowTest, "GAS_Demo.Buff.Selection.ServerFlow",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * 服务器流程：排队、编号校验、下标校验、施加失败保留本轮、成功后进入下一轮、
 * 重复请求被拒绝、本地请求走同一流程、没有角色时不消耗本轮、清空。
 */
bool FCCBuffSelectionServerFlowTest::RunTest(const FString& Parameters)
{
    using namespace CCBuffSelectionTest;
    const auto Initialization = UWorld::InitializationValues().AllowAudioPlayback(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num,
                                        &Initialization);
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }
    GEngine->CreateNewWorldContext(World->WorldType).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };

    APlayerController* Player = World->SpawnActor<APlayerController>();
    if (!TestNotNull(TEXT("PlayerController"), Player))
    {
        return false;
    }
    UCC_BuffSelectionTestComponent* Selection = NewObject<UCC_BuffSelectionTestComponent>(Player);
    Selection->RegisterComponent();
    TestTrue(TEXT("FindForPlayer 找到组件"), UCC_BuffSelectionComponent::FindForPlayer(Player) == Selection);

    TStrongObjectPtr<UCC_BuffOfferPool> Pool(MakePool(3));
    TestFalse(TEXT("空池不发起"), Selection->OfferBuffs(nullptr));
    TestTrue(TEXT("第一轮直接展示"), Selection->OfferBuffs(Pool.Get()));
    TestTrue(TEXT("第二轮排队"), Selection->OfferBuffs(Pool.Get()));
    const FCC_BuffOffer First = Selection->GetCurrentOffer();
    TestEqual(TEXT("当前是第一轮"), First.OfferId, 1);
    TestEqual(TEXT("三个选项"), First.Options.Num(), 3);
    TestEqual(TEXT("排队一轮"), Selection->GetQueuedOfferCount(), 1);

    TestEqual(TEXT("编号不符"), Selection->Resolve(99, 0), ECC_BuffChoiceResult::StaleOffer);
    TestEqual(TEXT("下标越界"), Selection->Resolve(1, 3), ECC_BuffChoiceResult::InvalidIndex);

    Selection->NextResult = ECC_BuffChoiceResult::ApplyFailed;
    TestEqual(TEXT("施加失败"), Selection->Resolve(1, 0), ECC_BuffChoiceResult::ApplyFailed);
    TestEqual(TEXT("施加失败保留本轮"), Selection->GetCurrentOffer().OfferId, 1);

    Selection->NextResult = ECC_BuffChoiceResult::Accepted;
    TestEqual(TEXT("施加成功"), Selection->Resolve(1, 1), ECC_BuffChoiceResult::Accepted);
    if (TestEqual(TEXT("记录一次施加"), Selection->Applied.Num(), 1))
    {
        TestTrue(TEXT("施加的是选中的那一项"), Selection->Applied[0] == First.Options[1]);
    }
    TestEqual(TEXT("进入第二轮"), Selection->GetCurrentOffer().OfferId, 2);
    TestEqual(TEXT("队列清空"), Selection->GetQueuedOfferCount(), 0);
    TestEqual(TEXT("重复提交第一轮被拒绝"), Selection->Resolve(1, 0), ECC_BuffChoiceResult::StaleOffer);

    // 单机下 RequestChoose 同步走 Server RPC → 校验 → 施加 → 下一轮。
    TestFalse(TEXT("本地请求编号不符"), Selection->RequestChoose(1, 0));
    TestTrue(TEXT("本地请求发出"), Selection->RequestChoose(2, 2));
    TestFalse(TEXT("结果返回后不再锁定"), Selection->IsChoiceInFlight());
    TestFalse(TEXT("全部选完"), Selection->HasPendingOffer());
    TestEqual(TEXT("共施加两次"), Selection->Applied.Num(), 2);

    // 正式施加逻辑：控制器没有 Pawn 时返回 NoTarget，本轮保留。
    Selection->bUseDefaultApply = true;
    TestTrue(TEXT("第三轮"), Selection->OfferBuffs(Pool.Get()));
    const int32 Third = Selection->GetCurrentOffer().OfferId;
    TestEqual(TEXT("没有角色"), Selection->Resolve(Third, 0), ECC_BuffChoiceResult::NoTarget);
    TestEqual(TEXT("没有角色时保留本轮"), Selection->GetCurrentOffer().OfferId, Third);

    // 自定义选项：无效项被丢弃，全部无效则不发起。
    TestFalse(TEXT("全无效选项不发起"), Selection->OfferOptions({Option(nullptr)}));
    TestTrue(TEXT("自定义选项排队"),
             Selection->OfferOptions({Option(nullptr), Option(UCC_BuffTestInstantEffect::StaticClass())}));
    TestEqual(TEXT("排队一轮"), Selection->GetQueuedOfferCount(), 1);

    Selection->ClearOffers();
    TestFalse(TEXT("清空当前一轮"), Selection->HasPendingOffer());
    TestEqual(TEXT("清空队列"), Selection->GetQueuedOfferCount(), 0);
    return true;
}
#endif
