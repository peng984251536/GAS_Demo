#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "UI/Toast/CC_ToastSettings.h"
#include "UI/Toast/CC_SurvivalToastRelay.h"
#include "Game/Survival/CC_SurvivalTags.h"

namespace
{
	FCC_ToastRequest MakeToast(const TCHAR* Title, int32 Priority = 0, FName Key = NAME_None)
	{
		FCC_ToastRequest Request;
		Request.Title = FText::FromString(Title);
		Request.Priority = Priority;
		Request.DedupeKey = Key;
		return Request;
	}
}

/** 队列：优先级排序、同优先级先到先出、合并键原地替换、超容量丢最低优先级中最早的。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCToastQueueTest, "GASDemo.UI.Toast.Queue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCToastQueueTest::RunTest(const FString& Parameters)
{
	FCC_ToastQueue Queue;
	Queue.Push(MakeToast(TEXT("A")), 8);
	Queue.Push(MakeToast(TEXT("B"), 5), 8);
	Queue.Push(MakeToast(TEXT("C")), 8);
	Queue.Push(MakeToast(TEXT("D"), 5), 8);
	TestEqual(TEXT("High priority first"), Queue.Peek(0).Title.ToString(), FString(TEXT("B")));
	TestEqual(TEXT("Same priority keeps arrival order"), Queue.Peek(1).Title.ToString(), FString(TEXT("D")));
	TestEqual(TEXT("Low priority after"), Queue.Peek(2).Title.ToString(), FString(TEXT("A")));

	FCC_ToastQueue Merge;
	Merge.Push(MakeToast(TEXT("Lv.2"), 0, TEXT("Level")), 8);
	Merge.Push(MakeToast(TEXT("Other")), 8);
	const bool bMerged = Merge.Push(MakeToast(TEXT("Lv.3"), 0, TEXT("Level")), 8);
	TestTrue(TEXT("Same key reports merge"), bMerged);
	TestEqual(TEXT("Merge does not add"), Merge.Num(), 2);
	TestEqual(TEXT("Merge keeps queue position and takes new content"), Merge.Peek(0).Title.ToString(), FString(TEXT("Lv.3")));

	FCC_ToastQueue Capped;
	Capped.Push(MakeToast(TEXT("Old")), 2);
	Capped.Push(MakeToast(TEXT("Important"), 9), 2);
	Capped.Push(MakeToast(TEXT("New")), 2);
	TestEqual(TEXT("Capacity respected"), Capped.Num(), 2);
	TestEqual(TEXT("High priority survives"), Capped.Peek(0).Title.ToString(), FString(TEXT("Important")));
	TestEqual(TEXT("Oldest low priority dropped"), Capped.Peek(1).Title.ToString(), FString(TEXT("New")));

	FCC_ToastRequest Out;
	TestTrue(TEXT("Pop returns head"), Capped.Pop(Out) && Out.Title.ToString() == TEXT("Important"));
	return true;
}

/** 模板：命名参数格式化，内置默认覆盖了生存模式的消息。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCCToastTemplateTest, "GASDemo.UI.Toast.Template",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCCToastTemplateTest::RunTest(const FString& Parameters)
{
	FCC_ToastTemplate Template;
	Template.Title = FText::FromString(TEXT("Wave {Value}"));
	Template.Message = FText::FromString(TEXT("{Player} ready"));
	FFormatNamedArguments Args;
	Args.Add(TEXT("Value"), FText::AsNumber(7));
	Args.Add(TEXT("Player"), FText::FromString(TEXT("Penn")));
	const FCC_ToastRequest Request = Template.ToRequest(Args, CCSurvivalTags::MsgWaveStarted);
	TestEqual(TEXT("Title formatted"), Request.Title.ToString(), FString(TEXT("Wave 7")));
	TestEqual(TEXT("Message formatted"), Request.Message.ToString(), FString(TEXT("Penn ready")));
	TestTrue(TEXT("Source tag set"), Request.SourceTag == CCSurvivalTags::MsgWaveStarted);

	FCC_ToastTemplate Builtin;
	TestTrue(TEXT("WaveStarted has builtin"), UCC_SurvivalToastRelay::GetBuiltinTemplate(CCSurvivalTags::MsgWaveStarted, Builtin));
	TestTrue(TEXT("LevelUp has builtin"), UCC_SurvivalToastRelay::GetBuiltinTemplate(CCSurvivalTags::MsgLevelUp, Builtin));
	TestFalse(TEXT("LevelUp dedupes"), Builtin.DedupeKey.IsNone());
	TestTrue(TEXT("WaveIncoming has builtin"), UCC_SurvivalToastRelay::GetBuiltinTemplate(CCToastTags::WaveIncoming, Builtin));
	TestFalse(TEXT("MatchEnded silent by default"), UCC_SurvivalToastRelay::GetBuiltinTemplate(CCSurvivalTags::MsgMatchEnded, Builtin));
	return true;
}

#endif
