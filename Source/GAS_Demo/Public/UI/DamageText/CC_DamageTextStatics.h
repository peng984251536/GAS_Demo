// 蓝图调用入口：伤害结算与飘字表现分开；HUD 中需预先放置一个 Damage Text Widget。
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageTextStatics.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;

/**
 * 伤害飘字的蓝图/C++ 调用入口。
 *
 * 解决一个具体问题：GameplayEffect 执行完扣血后，业务代码拿不到"实际扣了多少"。
 *
 * 为什么不在 PostGameplayEffectExecute 里取：
 *   Execution 会先把输出裁剪到当前生命值（100 伤害打 5 血目标，实际只扣 5），
 *   所以"理论伤害"和"实际掉血"不是一回事。而且 PostGameplayEffectExecute 在服务器执行，
 *   由它发 GameplayCue 需要自己处理复制和时序。
 *
 * 本方案的取舍：
 *   飘字显示的是**理论伤害**，通过本函数在应用 GE 时一并算出并随 GameplayCue 复制。
 *   好处是数值和表现一次传递、天然复制到所有客户端、不需要额外的伤害数值 RPC。
 *   代价是当目标血量不足时，飘字会显示理论值而非实际扣血值。
 *
 *   如果你需要"实际扣血值"（例如残血收割时显示 5 而不是 100），
 *   见 Docs/DamageText.zh-CN.md 里"显示实际扣血值"一节的改法。
 */
UCLASS()
class GAS_DEMO_API UCC_DamageTextStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 应用一次伤害 GE 并弹出对应飘字。
	 *
	 * 这是推荐的调用方式：替代直接调用 ASC->ApplyGameplayEffectSpecToTarget。
	 * 内部先应用 GE，成功后按攻击方当前 AttackPower 和传入倍率算出理论伤害，
	 * 作为 RawMagnitude 挂到单独执行的 GameplayCue 参数上。
	 *
	 * 只应在权威端调用；GameplayCue 会负责把表现复制到各客户端。
	 *
	 * @param SourceASC        攻击方能力系统；必须有效且为权威端。
	 * @param TargetASC        受击方能力系统。
	 * @param DamageEffectClass 伤害 GameplayEffect 类，通常是 CC_GE_LyraStyleDamage 的子类。
	 * @param SkillScale       攻击力系数，2 表示 200% 攻击力。
	 * @param FlatDamage       技能额外固定伤害。
	 * @param HitMultiplier    已判定的部位/暴击综合倍率；> 1 会标记为暴击飘字。
	 * @return 是否成功应用。
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Damage Text",
		meta = (AdvancedDisplay = "FlatDamage,HitMultiplier", AutoCreateRefTerm = "DamageEffectClass"))
	static bool ApplyDamageWithText(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		float SkillScale = 1.0f,
		float FlatDamage = 0.0f,
		float HitMultiplier = 1.0f);

	/**
	 * 只发一次受击表现（含飘字），不应用任何 GE。
	 *
	 * 用于伤害已经由其他途径结算、只想补一个表现的情况，
	 * 例如环境伤害、脚本事件、或者你已有自己的扣血流程。
	 * 若需让 GameplayCue 复制，应在权威端执行；客户端调用只触发本地表现。
	 * Amount 取绝对值；治疗请显式传 Heal 样式。此入口也可能播放 HitImpact 的特效与声音。
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Damage Text")
	static void ShowDamageText(
		UAbilitySystemComponent* TargetASC,
		float Amount,
		ECC_DamageTextStyle Style = ECC_DamageTextStyle::Normal);

	/**
	 * 从实际的生命值差值弹出飘字。
	 *
	 * 用于"显示实际扣血值"的场景：调用方在自己的扣血逻辑前后各读一次 Health，
	 * 把差值传进来。即可显示本次同步结算观测到的生命变化，
	 * 因为 Execution 的裁剪发生在 GAS 内部，外部无法在应用前预知。
	 */
	UFUNCTION(BlueprintCallable, Category = "GAS|Damage Text")
	static void ShowDamageTextFromDelta(
		UAbilitySystemComponent* TargetASC,
		float HealthBefore,
		float HealthAfter,
		bool bCritical = false);

private:
	/** 计算理论伤害，与 StandaloneDamageCalculator 的公式保持一致。 */
	static float ComputeTheoreticalDamage(
		UAbilitySystemComponent* SourceASC,
		float SkillScale,
		float FlatDamage,
		float HitMultiplier);

	/** 取飘字的世界锚点：优先用受击者包围盒中心，避免 Actor 原点在脚下。 */
	static FVector ResolveTextLocation(UAbilitySystemComponent* TargetASC);
};
