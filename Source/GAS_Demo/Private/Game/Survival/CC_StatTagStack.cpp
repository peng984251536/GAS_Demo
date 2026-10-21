#include "Game/Survival/CC_StatTagStack.h"

void FCC_StatTagStackContainer::AddStack(FGameplayTag Tag, int32 Count)
{
	if (!Tag.IsValid() || Count == 0)
	{
		return;
	}
	for (FCC_StatTagStack& Stack : Stacks)
	{
		if (Stack.Tag == Tag)
		{
			Stack.StackCount = FMath::Max(0, Stack.StackCount + Count);
			MarkItemDirty(Stack);
			return;
		}
	}
	if (Count < 0)
	{
		return;
	}
	FCC_StatTagStack& NewStack = Stacks.AddDefaulted_GetRef();
	NewStack.Tag = Tag;
	NewStack.StackCount = Count;
	MarkItemDirty(NewStack);
}

int32 FCC_StatTagStackContainer::GetStackCount(FGameplayTag Tag) const
{
	for (const FCC_StatTagStack& Stack : Stacks)
	{
		if (Stack.Tag == Tag)
		{
			return Stack.StackCount;
		}
	}
	return 0;
}

void FCC_StatTagStackContainer::Reset()
{
	if (!Stacks.IsEmpty())
	{
		Stacks.Reset();
		MarkArrayDirty();
	}
}
