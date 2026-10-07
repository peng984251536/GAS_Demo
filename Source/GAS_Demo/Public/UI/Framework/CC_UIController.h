#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CC_UIController.generated.h"

class UCC_ActivatableWidget;
class UWidget;
class UCC_UIModel;
class UCC_RootLayout;
class APlayerController;
class UWorld;

/**
 * 界面业务协调器：管理订阅、用户意图和系统调用；由所属视图持有，不跨地图保存。
 * 视图通常是入栈页面（CC_ActivatableWidget），也可以是世界覆盖层这类不进页面栈的常驻控件；
 * 后者没有焦点和关闭语义，CanHandleActions / RequestClose 对它们始终返回 false。
 */
UCLASS(BlueprintType, Blueprintable)
class GAS_DEMO_API UCC_UIController : public UObject
{
	GENERATED_BODY()
public:
	/** 默认使用轻量模型，功能控制器在构造函数中指定专用模型类。 */
	UCC_UIController();
	/** 返回视图所属世界，支持控制器蓝图调用世界上下文接口。 */
	virtual UWorld* GetWorld() const override;
	/** 视图内部调用一次：创建展示模型并记录所属视图。 */
	void Initialize(UWidget* InView, UObject* InContext);
	/**
	 * 不进页面栈的视图（如世界覆盖层）用它创建并初始化自己的控制器；类无效或为抽象类时返回空。
	 * 由视图持有返回值，在 Slate 构建时 Activate、释放时 Release。
	 */
	static UCC_UIController* CreateForView(UWidget* InView, TSubclassOf<UCC_UIController> ControllerClass, UObject* InContext = nullptr);
	/** 页面激活时绑定业务事件；重复调用不会重复订阅。 */
	void Activate();
	/** 页面失活时先作废会话令牌，再解除业务订阅。 */
	void Deactivate();
	/** 页面释放 Slate 时清理上下文和所有者，不能再用于旧页面。 */
	void Release();
	/** 已打开页面被再次 ShowScreen 时注入新上下文；激活中则重启会话，让 OnActivated 重新读取快照。 */
	void UpdateContext(UObject* InContext);
	/** 只读访问展示模型；Widget 通过具体模型的 Getter 读取快照。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	UCC_UIModel* GetModel() const { return Model; }
	/** 控制器所属玩家，尚未初始化或已释放时为空。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	APlayerController* GetPlayerController() const;
	/** 获取页面导航入口；不把根布局当作业务数据仓库。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	UCC_RootLayout* GetRootLayout() const;
	/** 当前仍在展示期间，不代表它是最高层可交互页面。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	bool IsActive() const { return bActive; }
	/** 拒绝失活、被覆盖或处于过渡中的页面发起用户操作；非页面视图始终返回 false。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	bool CanHandleActions() const;
	/** 当前激活会话标识；异步请求必须保存此值并在回调中校验。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	FGuid GetActivationToken() const { return ActivationToken; }
	/** 丢弃来自上次激活或已关闭页面的异步结果；不取消底层业务事务。 */
	UFUNCTION(BlueprintPure, Category="UI|Controller")
	bool IsActivationCurrent(FGuid Token) const { return bActive && Token.IsValid() && Token == ActivationToken; }
	/** 通过根布局请求关闭自己的页面。 */
	UFUNCTION(BlueprintCallable, Category="UI|Controller")
	bool RequestClose();
protected:
	/** 绑定业务事件并读取初始快照；C++ 子类覆盖后应调用 Super。 */
	virtual void OnActivated();
	/** 精确解除本控制器创建的订阅；C++ 子类覆盖后应调用 Super。 */
	virtual void OnDeactivated();
	/** 蓝图控制器的业务订阅/快照扩展点，不在此同步导航。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Controller")
	void ReceiveActivated();
	/** 蓝图控制器在此解除自己的委托、计时器或可取消任务。 */
	UFUNCTION(BlueprintImplementableEvent, Category="UI|Controller")
	void ReceiveDeactivated();
	/** 模型类型，由具体功能控制器指定；同一页面打开期间保持实例稳定。 */
	UPROPERTY(EditDefaultsOnly, Category="UI|Controller")
	TSubclassOf<UCC_UIModel> ModelClass;
	/** 入栈时传入的业务上下文；释放页面时清空。 */
	UPROPERTY(Transient, BlueprintReadOnly, Category="UI|Controller")
	TObjectPtr<UObject> Context;
protected:
	/** 页面（或常驻控件）只由外层框架持有，控制器的反向引用不延长视图生命。 */
	TWeakObjectPtr<UWidget> View;
	/** 视图是页面时返回它，否则为空；用于焦点、栈顶和关闭判断。 */
	UCC_ActivatableWidget* GetScreenView() const;
	/** 强引用展示模型，使它在页面被同层覆盖期间仍保留界面状态。 */
	UPROPERTY(Transient)
	TObjectPtr<UCC_UIModel> Model;
	/** 每次激活重新生成，避免旧回调污染重新打开的页面。 */
	FGuid ActivationToken;
	/** 幂等生命周期保护。 */
	bool bActive = false;
};
