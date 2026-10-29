#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "UI/Toast/CC_ToastTypes.h"
#include "CC_SurvivalToastRelay.generated.h"

class AGameStateBase;
class ACC_SurvivalGameState;
struct FCC_MatchMessage;

/**
 * 生存模式 → 提示弹窗的桥接（每个游戏世界一个，专用服务器不创建）。
 *
 * 监听 ACC_SurvivalGameState::OnMatchMessage（波次开始、全队升级、玩家倒下/复活……），
 * 按 Verb 查模板（项目设置 CC Toast → Message Toasts，没有则用内置默认），格式化后发给本机所有本地玩家。
 * 另外监听波次状态，在下一波开始前 WaveIncomingLeadSeconds 秒本地弹一次"敌人即将来袭"。
 *
 * 不修改生存模式本身：服务器发消息的方式、时机不变。其他玩法想加提示，直接调 UCC_ToastSubsystem::ShowToast。
 */
UCLASS()
class GAS_DEMO_API UCC_SurvivalToastRelay : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** 内置默认模板；返回 false 表示该标签没有内置提示。 */
	static bool GetBuiltinTemplate(FGameplayTag Tag, FCC_ToastTemplate& OutTemplate);
	/** 设置覆盖优先，其次内置。返回 false 表示不显示。 */
	static bool ResolveTemplate(FGameplayTag Tag, FCC_ToastTemplate& OutTemplate);

private:
	void HandleGameStateSet(AGameStateBase* GameState);
	void Bind(ACC_SurvivalGameState* GameState);
	void Unbind();

	UFUNCTION()
	void HandleMatchMessage(const FCC_MatchMessage& Message);
	UFUNCTION()
	void HandleWaveStateChanged();
	void FireWaveIncoming();

	/** 按模板的目标过滤发给本机每个本地玩家。 */
	void Dispatch(FGameplayTag Tag, const FCC_ToastTemplate& Template, const FFormatNamedArguments& Args, const class APlayerState* Target) const;

	TWeakObjectPtr<ACC_SurvivalGameState> BoundGameState;
	FDelegateHandle GameStateSetHandle;
	FTimerHandle WaveIncomingTimer;
	/** 已经为哪个 NextWaveServerTime 安排过预告，避免重复安排。 */
	double ScheduledForServerTime = -1.0;
	int32 ScheduledWave = 0;
};
