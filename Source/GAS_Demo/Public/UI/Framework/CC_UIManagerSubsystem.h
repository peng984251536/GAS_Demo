#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CC_UIManagerSubsystem.generated.h"
class UCC_UIPolicy;
class UCC_RootLayout;
class ULocalPlayer;
class APlayerController;
/** 根布局挂载完成，业务入口据此注入页面。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCC_UIRootReady, APlayerController*, Player, UCC_RootLayout*, Root);
/** 监听引擎本地玩家生命周期，不依赖项目 PlayerController 子类的 GameInstance UI 服务。 */
UCLASS(Config=Game)
class GAS_DEMO_API UCC_UIManagerSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 /** 专用服务器不创建 UI。 */
 virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
 /** 创建策略并订阅玩家、Controller 与地图生命周期。 */
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 /** 解除委托并最终释放根布局。 */
 virtual void Deinitialize() override;
 /** 查询持有的根布局，脱离视口期间对象仍存在。 */
 UFUNCTION(BlueprintPure, Category="UI")
 UCC_RootLayout* GetRootLayout(ULocalPlayer* Player) const;
 /** 任意本地 PlayerController 均可查询，不要求特定子类；未挂载返回空。 */
 UFUNCTION(BlueprintPure, Category="UI")
 static UCC_RootLayout* GetRootLayoutForPlayer(APlayerController* Player);
 /** 新建/重新挂载通知；订阅后也应查询已有布局，避免错过首次通知。 */
 UPROPERTY(BlueprintAssignable, Category="UI") FCC_UIRootReady OnRootLayoutReady;
private:
 /** 注册玩家 ControllerChanged，并处理已经存在的 Controller。 */
 void HandlePlayerAdded(ULocalPlayer* Player);
 /** 玩家真正退出时解除委托并释放布局。 */
 void HandlePlayerRemoved(ULocalPlayer* Player);
 /** 更新玩家上下文并创建或复用根布局。 */
 void HandleControllerChanged(APlayerController* Controller, ULocalPlayer* Player);
 /** 旧世界拆卸时清理页面与挂载，保留 Root UObject。 */
 void HandleWorldTearDown(UWorld* World);
 /** 普通/无缝切图完成后挂载，覆盖 Controller 未更换的情况。 */
 void HandleMapLoaded(UWorld* World);
 UPROPERTY(Config) TSoftClassPtr<UCC_UIPolicy> DefaultUIPolicyClass;
 UPROPERTY(Transient) TObjectPtr<UCC_UIPolicy> Policy;
 TMap<TWeakObjectPtr<ULocalPlayer>, FDelegateHandle> ControllerHandles;
 FDelegateHandle PlayerAddedHandle, PlayerRemovedHandle, WorldTearDownHandle, MapLoadedHandle;
};

