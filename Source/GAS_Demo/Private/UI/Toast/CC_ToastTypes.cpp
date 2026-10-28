#include "UI/Toast/CC_ToastTypes.h"

FCC_ToastRequest FCC_ToastTemplate::ToRequest(const FFormatNamedArguments& Args, FGameplayTag InSourceTag) const
{
	FCC_ToastRequest Request;
	Request.Title = Title.IsEmpty() ? FText::GetEmpty() : FText::Format(Title, Args);
	Request.Message = Message.IsEmpty() ? FText::GetEmpty() : FText::Format(Message, Args);
	Request.Style = Style;
	Request.Duration = Duration;
	Request.Priority = Priority;
	Request.DedupeKey = DedupeKey;
	Request.SourceTag = InSourceTag;
	Request.Icon = Icon;
	Request.SoundEvent = SoundEvent;
	return Request;
}

bool FCC_ToastQueue::Push(const FCC_ToastRequest& Request, int32 MaxSize)
{
	if (!Request.DedupeKey.IsNone())
	{
		for (FItem& Item : Items)
		{
			if (Item.Request.DedupeKey == Request.DedupeKey)
			{
				// 合并：内容取新的；优先级变化时重新排位。
				const bool bResort = Item.Request.Priority != Request.Priority;
				Item.Request = Request;
				if (bResort)
				{
					Items.StableSort([](const FItem& A, const FItem& B) { return A.Request.Priority > B.Request.Priority; });
				}
				return true;
			}
		}
	}

	FItem NewItem;
	NewItem.Request = Request;
	NewItem.Serial = NextSerial++;
	// 插到第一个优先级更低的元素之前，保持 (Priority 降序, Serial 升序)。
	int32 Insert = Items.Num();
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Items[Index].Request.Priority < Request.Priority)
		{
			Insert = Index;
			break;
		}
	}
	Items.Insert(MoveTemp(NewItem), Insert);

	// 容量：从队尾（最低优先级）开始丢；同优先级的队尾是最新到达的，所以先找最低优先级中最早的那条。
	while (MaxSize > 0 && Items.Num() > MaxSize)
	{
		const int32 LowestPriority = Items.Last().Request.Priority;
		int32 Victim = Items.Num() - 1;
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (Items[Index].Request.Priority == LowestPriority)
			{
				Victim = Index;
				break;
			}
		}
		Items.RemoveAt(Victim);
	}
	return false;
}

bool FCC_ToastQueue::Pop(FCC_ToastRequest& OutRequest)
{
	if (Items.IsEmpty())
	{
		return false;
	}
	OutRequest = MoveTemp(Items[0].Request);
	Items.RemoveAt(0);
	return true;
}
