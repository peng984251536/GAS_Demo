#pragma once

#include "Components/Widget.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "CC_DamageTextWidget.generated.h"

class UCC_DamageTextSubsystem;
class UCC_DamageTextEntryWidget;
class APlayerController;
class SCC_DamageTextLayer;

/** 只保存视图实例和条目 ID 的对应关系；业务数字数据仍由 Subsystem 管理。 */
USTRUCT()
struct FCC_DamageTextWidgetState
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UCC_DamageTextEntryWidget> Widget;
    // 保留 Slate 引用，复用 UMG 时也复用其 Slate 树，避免频繁重新 Construct。
    TSharedPtr<SWidget> SlateWidget;
    SConstraintCanvas::FSlot* Slot = nullptr;
    float LastAmount = 0.f;
    ECC_DamageTextStyle LastStyle = ECC_DamageTextStyle::Normal;
    FGameplayTag LastType;
    int32 LastFontSize = 0;
    bool bInitialized = false;
};

/**
 * 全屏飘字容器：一个 UMG 蓝图类、数据决定颜色；按条目 ID 复用控件。
 * Slate 只安排 UMG 子控件的位置和绘制层级，不再手工画另一套数字。
 * RootLayout 自动创建，不需要每次命中 AddToViewport，也不需要 Controller/Model。
 */
UCLASS(meta=(DisplayName="Damage Text Widget"))
class GAS_DEMO_API UCC_DamageTextWidget : public UWidget
{
    GENERATED_BODY()
public:
    /**
     * 配置默认数字模板和不参与鼠标命中测试的全屏容器。
     * @param ObjectInitializer 引擎提供的对象初始化参数。
     */
    UCC_DamageTextWidget(const FObjectInitializer& ObjectInitializer);
    /** 统一的数字蓝图类；不同伤害颜色通过数据切换，未配置时使用原生 TextBlock 模板。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text")
	TSubclassOf<UCC_DamageTextEntryWidget> EntryWidgetClass;
    /** 普通伤害和治疗字号。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text", meta=(ClampMin="1", ClampMax="512"))
	float FontSize = 28.f;
    /** 暴击字号；不同字号可能增加字体图集批次，不能保证一个 Draw Call。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text", meta=(ClampMin="1", ClampMax="512"))
	float CriticalFontSize = 40.f;
    /** 统一上升距离，单位为 HUD 布局坐标。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text", meta=(ClampMin="0"))
	float RiseHeight = 80.f;
    /** 空闲池上限；只减少创建/销毁，不直接保证 GPU 合批。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text", meta=(ClampMin="0"))
	int32 MaxPooledWidgets = 128;
    /** 简单数字共享绘制层；蓝图布局复杂或数字必须逐条遮挡时关闭，使用标准 Canvas 层级。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage Text")
	bool bShareDrawLayer = true;

    /**
     * 撤销订阅并释放活动/空闲 UMG 及 Slate 引用，防止切图留下旧世界控件。
     * @param bReleaseChildren 是否同时释放子控件资源。
     */
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    /**
     * 创建公共布局容器，并绑定当前本地玩家的数据源。
     * @return 公共 Slate 布局容器。
     */
    virtual TSharedRef<SWidget> RebuildWidget() override;
    /** 同步模板/样式设置并刷新数据源。 */
    virtual void SynchronizeProperties() override;
#if WITH_EDITOR
    /**
     * 返回设计器中的控件分类。
     * @return UI 分类名称。
     */
    virtual const FText GetPaletteCategory() override { return FText::FromString(TEXT("UI")); }
#endif
private:
    /**
     * 统一处理换绑和解绑；改变玩家/世界时清空控件池，不跨世界复用。
     * @param Source 新的飘字子系统，nullptr 表示解绑。
     * @param Player 当前本地玩家，解绑时为空。
     */
    void SetSource(UCC_DamageTextSubsystem* Source, APlayerController* Player);
    /** 按 ID 创建/复用/回收 UMG，只在数据变化时更新文字和通知蓝图。 */
    void SyncWidgets();
    /**
     * 使用一次公共投影，统一更新当前玩家数字的位置、透明度和缩放。
     * @param Geometry 当前全屏容器几何。
     */
    void UpdatePositions(const FGeometry& Geometry);
    /** 清空 Slate 子槽、活动控件和空闲池；模板/世界变化和资源释放共用。 */
    void ResetWidgets();
    TWeakObjectPtr<UCC_DamageTextSubsystem> BoundSubsystem;
    TWeakObjectPtr<APlayerController> RegisteredPlayer;
    FDelegateHandle UpdatedHandle;
    TSharedPtr<SCC_DamageTextLayer> DamageTextLayer;
    UPROPERTY(Transient) TMap<uint64, FCC_DamageTextWidgetState> ActiveWidgets;
    UPROPERTY(Transient) TArray<FCC_DamageTextWidgetState> PooledWidgets;
    UPROPERTY(Transient) TSubclassOf<UCC_DamageTextEntryWidget> CachedEntryClass;
    bool bSyncingWidgets = false;
    bool bResettingWidgets = false;
    friend class SCC_DamageTextLayer;
};
