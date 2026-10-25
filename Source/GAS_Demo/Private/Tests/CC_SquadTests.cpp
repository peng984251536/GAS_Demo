#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AI/Squad/CC_SquadTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCSquadFollowLeashTest, "GAS_Demo.AI.SquadFollowLeash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * FCC_SquadRules::PassesFollowLeash：跟随命令下的栓绳。
 * 默认半径：EngageRadius 800（新目标），LeashRadius 1200（当前目标），ReturnRadius 1500（自己离锚点）。
 * 锚点在原点，所有位置沿 X 轴。
 */
bool FCCSquadFollowLeashTest::RunTest(const FString& Parameters)
{
	FCC_SquadOrder Order;
	Order.Order = ECC_SquadOrder::Follow;
	const FVector Anchor = FVector::ZeroVector;
	const FVector Near(300.f, 0.f, 0.f);
	auto At = [](float X) { return FVector(X, 0.f, 0.f); };
	auto Passes = [&Order, &Anchor](const FVector& Self, const FVector& Candidate, bool bCurrent)
	{
		return FCC_SquadRules::PassesFollowLeash(Order, Anchor, Self, Candidate, bCurrent);
	};

	TestTrue(TEXT("新目标在 EngageRadius 内：接受"), Passes(Near, At(700.f), false));
	TestFalse(TEXT("新目标在 EngageRadius 外：拒绝"), Passes(Near, At(1000.f), false));
	TestTrue(TEXT("当前目标在 LeashRadius 内：继续打"), Passes(Near, At(1000.f), true));
	TestFalse(TEXT("当前目标超出 LeashRadius：放弃"), Passes(Near, At(1300.f), true));
	TestFalse(TEXT("自己超出 ReturnRadius：任何目标都拒绝"), Passes(At(1600.f), At(100.f), true));
	TestTrue(TEXT("只看水平距离：高度差不影响"), Passes(Near, FVector(700.f, 0.f, 900.f), false));

	Order.LeashRadius = 500.f;
	TestTrue(TEXT("LeashRadius 小于 EngageRadius 时当前目标至少按 EngageRadius 判断"), Passes(Near, At(700.f), true));
	return true;
}
#endif
