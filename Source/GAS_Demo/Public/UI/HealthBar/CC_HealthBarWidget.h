#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/WorldIndicator/CC_WorldIndicatorWidget.h"
#include "CC_HealthBarWidget.generated.h"

class UAbilitySystemComponent;
class UBorder;
class UImage;
class USizeBox;
struct FOnAttributeChangeData;

/**
 * 头顶血条控件（世界指示器控件）。Bind 时自己订阅角色 ASC 的 Health / MaxHealth，血量变化才刷新，
 * 显示比例在控件里平滑插值。没有设计树时用 C++ 搭一个简单血条（边框 + 背景 + 填充）。
 *
 * 自定义外观：派生蓝图做自己的设计树，实现 On Health Bar Updated 刷新进度条和文字；
 * 再在角色配置（Character|UI → Overhead Health Bar → Widget Class）里选它。
 * 满血 / 死亡隐藏作用在设计树根控件上，控件本身的位置和可见性由指示器层管理。
 */
UCLASS(Blueprintable)
class GAS_DEMO_API UCC_HealthBarWidget : public UUserWidget, public ICC_WorldIndicatorWidget
{
	GENERATED_BODY()
public:
	UCC_HealthBarWidget(const FObjectInitializer& ObjectInitializer);

	/** 满血时隐藏。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bHideWhenFull = false;
	/** 生命为 0 时隐藏。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar") bool bHideWhenDead = true;
	/** 显示比例追赶实际比例的速度；0 表示立即跳到目标。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar", meta=(ClampMin="0")) float InterpSpeed = 12.f;
	/** 默认外观：血条尺寸、填充色、背景色、边框。派生蓝图有自己的设计树时不使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar|Default Look") FVector2D BarSize = FVector2D(90.f, 8.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar|Default Look") FLinearColor FillColor = FLinearColor(0.25f, 0.8f, 0.3f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar|Default Look") FLinearColor BackgroundColor = FLinearColor(0.03f, 0.03f, 0.03f, 0.85f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar|Default Look") FLinearColor BorderColor = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health Bar|Default Look", meta=(ClampMin="0")) float BorderWidth = 1.f;

	UFUNCTION(BlueprintPure, Category="Health Bar") AActor* GetTargetActor() const { return TargetActor.Get(); }
	UFUNCTION(BlueprintPure, Category="Health Bar") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category="Health Bar") float GetMaxHealth() const { return MaxHealth; }
	/** 平滑后的显示比例 0..1。 */
	UFUNCTION(BlueprintPure, Category="Health Bar") float GetDisplayedFraction() const { return DisplayedFraction; }

	virtual void BindIndicator_Implementation(UCC_WorldIndicator* Indicator) override;
	virtual void UnbindIndicator_Implementation() override;
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	/** 绑定到新角色时调用（含对象池复用），可在这里读取名字、等级等。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Health Bar")
	void OnHealthBarBound(AActor* Target);
	/** 血量或显示比例变化时调用；插值期间每帧调用。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Health Bar")
	void OnHealthBarUpdated(float CurrentHealth, float CurrentMaxHealth, float Fraction);
private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void ReadVitals();
	/** 刷新默认外观、根控件显隐，并通知蓝图。 */
	void RefreshView();
	float TargetFraction() const { return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f; }

	TWeakObjectPtr<AActor> TargetActor;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle HealthHandle, MaxHealthHandle;
	float Health = 0.f, MaxHealth = 0.f, DisplayedFraction = 0.f;

	UPROPERTY(Transient) TObjectPtr<UBorder> Frame;
	UPROPERTY(Transient) TObjectPtr<USizeBox> BarBox;
	UPROPERTY(Transient) TObjectPtr<USizeBox> FillBox;
	UPROPERTY(Transient) TObjectPtr<UImage> Fill;
};
