#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Engine/EngineTypes.h"
#include "CC_HealthBarSource.generated.h"

class ACC_BaseCharacter;
class UCC_WorldIndicator;

/**
 * 头顶血条的数据来源（每个本地玩家一份）。UI 侧主动发现角色，玩法代码不知道血条的存在：
 *  - 订阅 ACC_BaseCharacter::OnAnyCharacterReady，读取角色配置决定是否显示、用哪个控件；
 *  - 为角色添加一个跟随它的世界指示器，角色 EndPlay 时移除；
 *  - 本地玩家自己的角色不显示（自身血量在 HUD 上）。
 * 血量的读取和刷新由血条控件自己完成（订阅 ASC），这里不按帧做任何事。
 */
UCLASS()
class GAS_DEMO_API UCC_HealthBarSource : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** 换了 PlayerController（如切图后）时补扫一遍当前世界里已经就绪的角色。 */
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	/** 当前有血条的角色数量，供调试和测试。 */
	int32 GetHealthBarCount() const { return HealthBars.Num(); }
	/** 为角色添加血条（已存在、不需要显示或不属于本玩家世界时忽略）。 */
	void AddHealthBar(ACC_BaseCharacter* Character);
private:
	UFUNCTION() void HandleCharacterEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	void RemoveHealthBar(AActor* Actor);
	void ScanWorld();

	/** 角色 → 它的血条指示器。指示器由 UCC_WorldIndicatorSubsystem 持有，这里只记弱引用。 */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<UCC_WorldIndicator>> HealthBars;
	FDelegateHandle ReadyHandle;
};
