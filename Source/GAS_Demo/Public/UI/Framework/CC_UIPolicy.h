#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CC_UIPolicy.generated.h"
class UCC_RootLayout;
class ULocalPlayer;
class APlayerController;
/** 对应 Lyra GameUIPolicy：布局归 LocalPlayer，Controller/世界变化只改变页面和挂载。 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_UIPolicy : public UObject
{
 GENERATED_BODY()
public:
 /** 指定原生布局默认类。 */
 UCC_UIPolicy();
 /** 有效本地玩家首次创建根布局，此后复用同一对象并更新上下文。 */
 UCC_RootLayout* EnsureRoot(ULocalPlayer* Player);
 /** 查询持有对象，不隐式创建。 */
 UCC_RootLayout* GetRootLayout(ULocalPlayer* Player) const;
 /** 清空旧世界页面并脱离视口，保留根布局。 */
 void DetachWorld(UWorld* World);
 /** 本地玩家真正退出时永久释放。 */
 void ReleasePlayer(ULocalPlayer* Player);
 /** GameInstance 结束时永久释放全部根布局。 */
 void ReleaseAll();
protected:
 /** 唯一的根布局类配置入口，不接受 Controller 覆盖。 */
 UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UCC_RootLayout> DefaultRootLayoutClass;
private:
 /** 最终释放，不用于普通切图。 */
 void ReleaseRoot(UCC_RootLayout* Root);
 UPROPERTY(Transient) TMap<TObjectPtr<ULocalPlayer>, TObjectPtr<UCC_RootLayout>> Roots;
 /** 独立记录实际挂载世界和控制器，不能使用切图后已经变化的动态玩家上下文比较。 */
 TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<UWorld>> AttachedWorlds;
 TMap<TWeakObjectPtr<ULocalPlayer>, TWeakObjectPtr<APlayerController>> AttachedControllers;
 bool bUpdating = false;
};
