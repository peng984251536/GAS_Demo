#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "CC_ToastSubsystem.generated.h"

class APlayerController;
class UCC_ToastHostWidget;
class UWorld;

/**
 * 提示弹窗的每玩家服务（每个本地玩家一个）。业务代码请用 UCC_ToastStatics::ShowToast，不要自己 CreateWidget。
 *
 * 职责：确保宿主控件已挂进该玩家根布局的通知层；根布局未就绪时先缓存；用 Ticker 按真实时间驱动宿主。
 * 切图时根布局会清空通知层，宿主随之失效，下次 ShowToast 自动重建。
 */
UCLASS()
class GAS_DEMO_API UCC_ToastSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 弹出一条提示。返回 false 表示请求无效（标题和正文都为空）。 */
	UFUNCTION(BlueprintCallable, Category="UI|Toast")
	bool ShowToast(const FCC_ToastRequest& Request);

	/** 清掉正在显示和排队的全部提示。 */
	UFUNCTION(BlueprintCallable, Category="UI|Toast")
	void ClearToasts();

	/** 当前宿主（可能为空）。 */
	UFUNCTION(BlueprintPure, Category="UI|Toast")
	UCC_ToastHostWidget* GetHost() const { return Host.Get(); }

	static UCC_ToastSubsystem* Get(const APlayerController* Player);

private:
	bool Tick(float DeltaTime);
	void EnsureTicking();
	UCC_ToastHostWidget* EnsureHost();
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	/** 宿主由通知层 Overlay 持有，这里只弱引用。 */
	TWeakObjectPtr<UCC_ToastHostWidget> Host;
	TArray<FCC_ToastRequest> Pending;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle WorldCleanupHandle;
};

