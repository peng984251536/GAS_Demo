#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "CC_BatchedHealthBarSubsystem.generated.h"

class UAbilitySystemComponent;
class APlayerController;

/** 蓝图可配置的血条样式与显示规则。每个角色只保存数据，不创建组件、UMG 实例或渲染目标。 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_HealthBarOptions
{
	GENERATED_BODY()
	/** 在角色位置上叠加的世界空间偏移，单位厘米；不随角色旋转，用于把锚点移到头顶。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FVector WorldOffset = FVector(0, 0, 120);
	/** 血条宽高，单位为 HUD 布局单位，不是世界厘米。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FVector2D Size = FVector2D(90, 8);
	/** 该角色的填充颜色；仅改变顶点颜色，所有血条仍共用同一个 Brush。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") FLinearColor Color = FLinearColor(0.25f, 0.8f, 0.3f);
	/** 最远显示距离，单位为世界厘米；0 表示不按距离剔除。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar", meta=(ClampMin="0")) float MaxDistance = 5000;
	/** 生命值达到最大值时隐藏，仍保留数据与订阅。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bHideWhenFull = false;
	/** 生命值小于等于 0 时隐藏；隐藏不等于移除条目。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bHideWhenDead = true;
	/** 开启后自动订阅角色 ASC 的 Health / MaxHealth；未就绪时定期重试。关闭后由蓝图手动推送数值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bUseGAS = true;
};

/** 运行时条目，仅供 C++ 管理器和绘制层使用，不承担复制或 UObject 所有权。 */
struct FCC_HealthBarEntry
{
	/** 弱引用目标角色，不阻止角色销毁；每次 Tick 检查有效性。 */
	TWeakObjectPtr<AActor> Actor;
	/** 当前订阅的数据源；ASC 更换时先解绑旧源再绑定新源。 */
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	/** 当前角色独立的显示配置。 */
	FCC_HealthBarOptions Options;
	/** 保存本条目创建的委托句柄，只移除自己的监听，不影响其他 UI。 */
	FDelegateHandle HealthHandle, MaxHealthHandle;
	/** Health / MaxHealth 为最新数值；DisplayedFraction 是逐帧插值后的实际绘制比例。 */
	float Health = 0, MaxHealth = 0, DisplayedFraction = 0;
	/** 蓝图控制的显隐开关，关闭后仍跟踪血量变化。 */
	bool bVisible = true;
	/** 是否已经取得有效数值；首次就绪直接设置显示比例，避免从空血条开始播放。 */
	bool bReady = false;
};

/**
 * 每个本地玩家一份血条数据管理器，负责 GAS 订阅、条目清理和血量插值，不负责绘制。
 * 界面侧由 CC_HealthBarOverlayController 订阅 OnEntriesUpdated，转换为展示模型后交给绘制层。
 * 分屏玩家的数据相互独立；联机时需在各本地客户端注册，注册操作不进行网络复制。
 * 子系统由引擎随 LocalPlayer 创建，但不会自动创建 HUD Widget，也不会扫描场景中的敌人。
 */
UCLASS()
class GAS_DEMO_API UCC_BatchedHealthBarSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	/** 从本地 PlayerController 取得管理器；无本地玩家的控制器返回 nullptr。 */
	UFUNCTION(BlueprintPure, Category="UI|Batched Health Bars")
	static UCC_BatchedHealthBarSubsystem* GetHealthBarManager(APlayerController* PlayerController);
	/** 注册角色或更新已有配置，不会重复创建条目。返回 false 表示角色、世界或配置无效；成功注册不代表 GAS 已就绪。 */
	UFUNCTION(BlueprintCallable, Category="UI|Batched Health Bars")
	bool RegisterHealthBar(AActor* Actor, const FCC_HealthBarOptions& Options);
	/** 移除指定角色并解除属性监听；重复调用安全。 */
	UFUNCTION(BlueprintCallable, Category="UI|Batched Health Bars") void UnregisterHealthBar(AActor* Actor);
	/** 手动模式更新数值，要求 bUseGAS=false；注册后首次调用负责初始化显示比例。 */
	UFUNCTION(BlueprintCallable, Category="UI|Batched Health Bars") bool UpdateHealthBar(AActor* Actor, float Health, float MaxHealth);
	/** 临时隐藏或显示已注册血条，不改变注册状态。 */
	UFUNCTION(BlueprintCallable, Category="UI|Batched Health Bars") void SetHealthBarVisible(AActor* Actor, bool bVisible);
	/** 清空本地玩家的所有条目并解绑，可在退出战斗时由蓝图调用。 */
	UFUNCTION(BlueprintCallable, Category="UI|Batched Health Bars") void ClearHealthBars();
	/** 注册条目总数，包含隐藏、未就绪或超出显示距离的条目。 */
	UFUNCTION(BlueprintPure, Category="UI|Batched Health Bars") int32 GetRegisteredCount() const { return Entries.Num(); }
	/**
	 * 血条控制器激活/失活时登记，返回登记后的数量。同一本地玩家出现多个绘制层通常意味着
	 * 旧的手动 Add to Player Screen 与根布局托管的血条层同时存在，血条会被画两遍。
	 */
	int32 AddDrawLayer() { return ++DrawLayerCount; }
	void RemoveDrawLayer() { DrawLayerCount = FMath::Max(0, DrawLayerCount - 1); }
	/**
	 * 条目更新通知：每次 Tick 完成插值与清理后、以及注册/移除/显隐/手动推送后广播。
	 * 只供界面控制器把条目转换为展示快照，回调中不要再注册或移除条目。
	 */
	FSimpleMulticastDelegate OnEntriesUpdated;
	/** 控制器只读访问；不要跨注册、移除或 Tick 保存数组元素的引用。 */
	const TArray<FCC_HealthBarEntry>& GetEntries() const { return Entries; }
	/** 集中完成存活检查、低频 GAS 重绑和每帧血量插值。 */
	virtual void Tick(float DeltaTime) override;
	/** 排除模板、无数据及非游戏世界，避免空闲时进行循环处理。 */
	virtual bool IsTickable() const override;
	/** 提供性能统计名称，用于观察管理器的 Tick 开销。 */
	virtual TStatId GetStatId() const override;
	/** 将 Tick 限定在该 LocalPlayer 当前所属的世界，避免多世界混用。 */
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	/** 本地玩家子系统结束时释放全部订阅。 */
	virtual void Deinitialize() override;
private:
	/** 所有角色共用的数据列表，不保存逐角色的 UI 对象。 */
	TArray<FCC_HealthBarEntry> Entries;
	/** 当前已构建的绘制层数量，仅用于重复挂载诊断。 */
	int32 DrawLayerCount = 0;
	/** GAS 数据源检查计时器；每 0.25 秒检查一次，不逐帧读取属性。 */
	float BindingTimer = 0;
	/** 按角色查找当前条目；返回指针只适合本次操作内使用。 */
	FCC_HealthBarEntry* Find(AActor* Actor);
	/** 检查属性集、绑定或替换 ASC，并推送初始数值。 */
	void Bind(FCC_HealthBarEntry& Entry);
	/** 按句柄解除监听并重置弱引用，ASC 已销毁时也可安全调用。 */
	void Unbind(FCC_HealthBarEntry& Entry);
	/** 在绑定初次成功或属性变化事件中读取生命值与最大值。 */
	void ReadVitals(FCC_HealthBarEntry& Entry);
};
