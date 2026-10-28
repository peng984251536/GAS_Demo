#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "CC_ToastStatics.generated.h"

/**
 * 提示弹窗的静态调用入口（C++ / 蓝图通用），任何地方一行调用：
 *
 *   UCC_ToastStatics::ShowToastText(this, LOCTEXT("LvUp", "全队升级"), LOCTEXT("LvUpMsg", "等级 5"), ECC_ToastStyle::Success);
 *
 * WorldContext 决定发给谁：
 *   - 本地 PlayerController，或被本地玩家控制的 Pawn / 其组件，或属于本地玩家的 Widget → 只发给那个玩家；
 *   - 其他对象（GameState、GameMode、AI、子系统……）→ 发给本机所有本地玩家（分屏每人一份）。
 * 只影响本机界面；在专用服务器上调用无效果。需要所有客户端都看到的事件，走生存模式的对局消息（见文档第 4 节）。
 */
UCLASS()
class GAS_DEMO_API UCC_ToastStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 弹出一条提示（完整参数）。返回是否至少发给了一个本地玩家。 */
	UFUNCTION(BlueprintCallable, Category="CC|Toast", meta=(WorldContext="WorldContextObject"))
	static bool ShowToast(const UObject* WorldContextObject, const FCC_ToastRequest& Request);

	/** 只填文字的简化版本。Duration <= 0 使用默认显示时长。 */
	UFUNCTION(BlueprintCallable, Category="CC|Toast", meta=(WorldContext="WorldContextObject", AdvancedDisplay="Duration,DedupeKey"))
	static bool ShowToastText(const UObject* WorldContextObject, FText Title, FText Message,
		ECC_ToastStyle Style = ECC_ToastStyle::Info, float Duration = 0.f, FName DedupeKey = NAME_None);

	/** 清空提示（规则同上：本地玩家相关对象只清该玩家，否则清全部本地玩家）。 */
	UFUNCTION(BlueprintCallable, Category="CC|Toast", meta=(WorldContext="WorldContextObject"))
	static void ClearToasts(const UObject* WorldContextObject);
};
