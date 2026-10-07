#include "UI/WorldOverlay/CC_WorldOverlayController.h"
#include "Components/Widget.h"
#include "GAS_Demo.h"

void UCC_WorldOverlayController::WarnIfDuplicateLayer(int32 LayerCount, const TCHAR* LayerName, const TCHAR* Hint) const
{
	if (LayerCount > 1)
	{
		UE_LOG(LogGAS_Demo, Warning, TEXT("同一本地玩家存在多个%s绘制层（%s），会被重复绘制。%s"),
			LayerName, *GetPathNameSafe(View.Get()), Hint);
	}
}
