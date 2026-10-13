#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CC_BuffBarWidget.generated.h"

class ACC_BaseCharacter;
class UCC_BuffComponent;
class UHorizontalBox;
class UTextBlock;

/**
 * 可选的角色 buff 栏，显示配置中的图标、名称以及 GAS 实际层数和剩余时间。
 * 只观察角色组件同步的 UI 数据，不修改效果或属性；更换角色时重新绑定观察对象。
 */
UCLASS()
class GAS_DEMO_API UCC_BuffBarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /**
     * 更换当前观察的角色，解除旧角色通知并立即读取新角色的活动 buff。
     * 角色重生或切换控制对象后应再次调用，不保存对已销毁角色的强引用。
     */
    UFUNCTION(BlueprintCallable, Category="Buff")
    void SetObservedCharacter(ACC_BaseCharacter* Character);
protected:
    /**
     * 创建默认的 buff 展示容器，后续变化只更新显示内容，不修改角色状态。
     */
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UPROPERTY(Transient) TObjectPtr<UHorizontalBox> BuffBox;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Timers;
    TArray<int32> InstanceIds;
    TWeakObjectPtr<UCC_BuffComponent> Observed;
    /**
     * 根据角色最新 UI 快照重建图标、名称与层数，抑制中的效果以低透明度显示。
     * 倒计时在控件更新时读取服务器到期时间，无需创建额外效果计时器。
     */
    UFUNCTION() void RefreshBuffs();
};
