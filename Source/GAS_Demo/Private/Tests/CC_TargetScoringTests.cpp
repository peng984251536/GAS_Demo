#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AI/Targeting/CC_TargetScoring.h"

namespace CCTargetScoringTests
{
	FCC_TargetCandidate Make(float Distance, int32 MeleeAttackers = 0, bool bTargetsSelf = false, bool bBlocked = false)
	{
		FCC_TargetCandidate Candidate;
		Candidate.Distance = Distance;
		Candidate.MeleeAttackers = MeleeAttackers;
		Candidate.bTargetsSelf = bTargetsSelf;
		Candidate.bBlocked = bBlocked;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCTargetScoringTest, "GAS_Demo.AI.TargetScoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * FCC_TargetScoring 纯逻辑：距离、拥挤软上限、弓兵忽略拥挤、回敬、被挡惩罚、迟滞。
 * 默认参数：软上限 3，拥挤惩罚 300，回敬 150，被挡 600；迟滞 200。
 */
bool FCCTargetScoringTest::RunTest(const FString& Parameters)
{
	using namespace CCTargetScoringTests;
	const FCC_TargetScoringParams Params;
	const float Hysteresis = 200.f;
	auto Select = [&Params, Hysteresis](const TArray<FCC_TargetCandidate>& Candidates, int32 Current = INDEX_NONE,
		bool bMelee = true)
	{
		return FCC_TargetScoring::SelectBest(Candidates, Current, Params, Hysteresis, bMelee);
	};

	TestEqual(TEXT("没有候选"), Select({}), INDEX_NONE);
	TestEqual(TEXT("只有距离时选最近的"), Select({Make(800.f), Make(500.f)}), 1);

	// 近的目标已有 5 个近战（超出软上限 2 人，+600 → 1100），远的空闲（900）：选远的。
	TestEqual(TEXT("超出软上限后选稍远但空闲的"), Select({Make(500.f, 5), Make(900.f)}), 1);
	// 刚好 3 人，不超出软上限，不受影响。
	TestEqual(TEXT("未超出软上限时不受影响"), Select({Make(500.f, 3), Make(900.f)}), 0);
	TestEqual(TEXT("弓兵不受拥挤影响"), Select({Make(500.f, 5), Make(900.f)}, INDEX_NONE, false), 0);

	TestEqual(TEXT("等距时优先回敬正在打自己的"), Select({Make(500.f), Make(500.f, 0, true)}), 1);
	TestEqual(TEXT("回敬优惠有限：远太多时仍选近的"), Select({Make(300.f), Make(600.f, 0, true)}), 0);

	TestEqual(TEXT("被挡的目标被惩罚"), Select({Make(300.f, 0, false, true), Make(700.f)}), 1);
	TestEqual(TEXT("没有其他候选时仍选被挡的目标"), Select({Make(300.f, 0, false, true)}), 0);

	TestEqual(TEXT("优势 150 < 迟滞 200：保持当前目标"), Select({Make(500.f), Make(650.f)}, 1), 1);
	TestEqual(TEXT("优势 250 > 迟滞 200：切换"), Select({Make(400.f), Make(650.f)}, 1), 0);
	TestEqual(TEXT("当前目标被挡后（+600）切换到更远的"), Select({Make(300.f, 0, false, true), Make(700.f)}, 0), 1);
	TestEqual(TEXT("当前下标无效时直接取最优"), Select({Make(500.f), Make(650.f)}, 5), 0);

	const FCC_TargetCandidate Crowded = Make(500.f, 5, true, true);
	TestEqual(TEXT("各项叠加：500 + 600 − 150 + 600"),
		FCC_TargetScoring::Score(Crowded, Params, true), 1550.f);
	return true;
}
#endif
