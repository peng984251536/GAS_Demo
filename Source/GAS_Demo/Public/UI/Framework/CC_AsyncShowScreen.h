#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "CC_AsyncShowScreen.generated.h"

class UCC_RootLayout;
class UCC_ActivatableWidget;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCC_AsyncScreenResult, UCC_ActivatableWidget*, Screen);

/** 可取消的软引用页面加载节点；加载完成后等待栈空闲再入栈，切图时取消并解除输入过滤。 */
UCLASS(meta=(ExposedAsyncProxy=AsyncAction))
class GAS_DEMO_API UCC_AsyncShowScreen : public UCancellableAsyncAction
{
	GENERATED_BODY()
public:
	/** 创建蓝图异步节点；Context 在等待期间保持有效，Completed 表示已入栈而非动画结束。 */
	UFUNCTION(BlueprintCallable, Category="UI", meta=(BlueprintInternalUseOnly="true", DisplayName="Show Screen Async"))
	static UCC_AsyncShowScreen* ShowScreenAsync(UCC_RootLayout* RootLayout, FGameplayTag Layer, TSoftClassPtr<UCC_ActivatableWidget> ScreenClass, UObject* Context, bool bSuspendInput = true);
	/** 启动加载并监听根布局关闭；蓝图会自动调用，C++ 调用者须在绑定回调后调用。 */
	virtual void Activate() override;
	/** 取消后不再发送成功/失败事件，也不关闭已经成功入栈的页面。 */
	virtual void Cancel() override;
	/** 显式报告请求状态；UE 5.6 基类取消后仍保留注册实例的弱指针，不能只依赖基类判断。 */
	virtual bool IsActive() const override { return bStarted && !bFinished && Super::IsActive(); }
	/** 仅成功入栈时返回页面实例；同层同类遵守同步入口的复用规则。 */
	UPROPERTY(BlueprintAssignable) FCC_AsyncScreenResult Completed;
	/** 无效层/类或加载失败时触发；Screen 为空。取消请求不触发该事件。 */
	UPROPERTY(BlueprintAssignable) FCC_AsyncScreenResult Failed;
private:
	/** 使用核心 Ticker 等待加载与过渡，世界暂停时仍可完成。 */
	bool TickRequest(float DeltaTime);
	/** 先解除句柄、过滤和计时，再发结果，避免回调导航造成重入。 */
	void Finish(UCC_ActivatableWidget* Screen);
	/** 幂等地撤销本请求拥有的所有资源，不影响其他加载请求。 */
	void Cleanup();
	TWeakObjectPtr<UCC_RootLayout> Root;
	UPROPERTY(Transient) TSoftClassPtr<UCC_ActivatableWidget> ClassToLoad;
	UPROPERTY(Transient) TObjectPtr<UObject> DisplayContext;
	FGameplayTag TargetLayer;
	TSharedPtr<FStreamableHandle> LoadHandle;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle ShutdownHandle;
	FName InputToken;
	bool bSuspend = true;
	bool bStarted = false;
	bool bFinished = false;
};
