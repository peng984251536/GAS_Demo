// 调用链：蓝图/技能 → 本工具构造 GameplayCue 参数 → HitImpact Cue → 世界子系统记录飘字。
#include "UI/DamageText/CC_DamageTextStatics.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attribute/CC_AttributeSet.h"
#include "Damage/GASDamageExecutionBase.h"
#include "UI/DamageText/CC_DamageTextSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"

namespace DamageTextCueTags
{
	// 与 CC_GameplayCue_HitImpact 注册的标签一致；受击表现统一走这一条 Cue。
	UE_DEFINE_GAMEPLAY_TAG_STATIC(HitImpact, "GameplayCue.Combat.HitImpact");
	// 标记本次为暴击，供表现层放大字号与变色。
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Critical, "GameplayCue.DamageText.Critical");
	// 标记本次为治疗，供表现层改色与加号。
	UE_DEFINE_GAMEPLAY_TAG_STATIC(Heal, "GameplayCue.DamageText.Heal");
}

// 按当前攻击力、技能系数、固定伤害和综合倍率计算显示数值，不读取目标实际掉血量。
float UCC_DamageTextStatics::ComputeTheoreticalDamage(
	UAbilitySystemComponent* SourceASC,
	float SkillScale,
	float FlatDamage,
	float HitMultiplier)
{
	if (!SourceASC)
	{
		return 0.0f;
	}

	// 读取攻击方当前已聚合的 AttackPower（包含 Buff 加成）。
	// 这与 UGASDamageExecutionBase 的捕获口径一致：都是"数值攻击力"。
	const float AttackPower = SourceASC->GetNumericAttribute(
		UCC_AttributeSet::GetAttackPowerAttribute());

	// 与 StandaloneDamageCalculator 相同的公式，只是省略了防御和承伤倍率
	// （当前项目没有这两项属性）。这里的目的是给飘字一个合理的显示值，
	// 真正的权威扣血仍由 GE 的 Execution 完成，两者使用同一套输入，因此数值一致。
	const float Raw = AttackPower * SkillScale + FlatDamage;
	return FMath::Max(0.0f, Raw * HitMultiplier);
}

// 用 Avatar 的碰撞包围盒中心加上向上偏移，生成一次命中的固定世界锚点。
FVector UCC_DamageTextStatics::ResolveTextLocation(UAbilitySystemComponent* TargetASC)
{
	if (!TargetASC)
	{
		return FVector::ZeroVector;
	}

	AActor* Avatar = TargetASC->GetAvatarActor();
	if (!Avatar)
	{
		return FVector::ZeroVector;
	}

	// 用包围盒中心而不是 Actor 原点：角色的原点通常在两脚之间，
	// 直接把数字画在那里会贴着地面，视觉上不对。
	FVector Origin;
	FVector Extent;
	Avatar->GetActorBounds(true, Origin, Extent);

	// 上移到头顶附近，避免数字糊在身上。
	return Origin + FVector(0.0f, 0.0f, Extent.Z * 0.6f);
}

// 权威端先应用 GE，成功后单独发送受击 Cue；显示数值使用本函数的理论伤害公式。
bool UCC_DamageTextStatics::ApplyDamageWithText(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	float SkillScale,
	float FlatDamage,
	float HitMultiplier)
{
	if (!SourceASC || !TargetASC || !DamageEffectClass)
	{
		return false;
	}

	// 伤害结算必须发生在权威端。客户端调用只会产生一个无意义的本地 Spec。
	if (!SourceASC->IsOwnerActorAuthoritative())
	{
		return false;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceASC->GetOwnerActor(), SourceASC->GetAvatarActor());

	FGameplayEffectSpecHandle Spec =
		SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	// 填入与 UGASDamageExecutionBase 约定的 SetByCaller 参数。
	// 直接引用 GASDamageExecutionBase.h 里原生注册的 Tag 定义，
	// 避免用字符串查表——拼错字符串不会编译报错，而这里写错会。
	Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::SkillScale, SkillScale);
	Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::FlatDamage, FlatDamage);
	Spec.Data->SetSetByCallerMagnitude(StandaloneDamageTags::HitMultiplier, HitMultiplier);

	// ApplyGameplayEffectSpecToTarget 返回的是 FActiveGameplayEffectHandle，不是 bool。
	// 句柄无效表示 Spec 被拒绝（例如目标无法接受该 GE）。
	const FActiveGameplayEffectHandle AppliedHandle =
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	const bool bApplied = AppliedHandle.WasSuccessfullyApplied();

	if (bApplied)
	{
		// 表现层单独发一条 Cue，把算好的伤害值挂上去。
		//
		// 注意：这里发的是"理论伤害"。目标血量不足时，实际扣血会被 Execution 裁剪，
		// 飘字仍显示理论值——这个取舍在头文件里说明了，改法见 Docs/DamageText.zh-CN.md。
		const float Damage = ComputeTheoreticalDamage(
			SourceASC, SkillScale, FlatDamage, HitMultiplier);

		// HitMultiplier > 1 视为暴击。当前项目没有独立的暴击标记，
		// 由技能侧通过倍率表达；如果你的暴击走别的方式，改用 ShowDamageText 自行指定样式。
		const ECC_DamageTextStyle Style = HitMultiplier > 1.0f
			? ECC_DamageTextStyle::Critical
			: ECC_DamageTextStyle::Normal;

		FGameplayCueParameters CueParams;
		CueParams.RawMagnitude = Damage;          // 传递给表现层的数值
		CueParams.Location = ResolveTextLocation(TargetASC);
		CueParams.Instigator = SourceASC->GetAvatarActor();
		CueParams.EffectCauser = SourceASC->GetAvatarActor();

		if (Style == ECC_DamageTextStyle::Critical)
		{
			CueParams.AggregatedSourceTags.AddTag(DamageTextCueTags::Critical);
		}

		TargetASC->ExecuteGameplayCue(DamageTextCueTags::HitImpact, CueParams);
	}

	return bApplied;
}

// 只构造并执行表现 Cue，不修改属性；Style 决定普通、暴击或治疗，Amount 的符号在此被取绝对值。
void UCC_DamageTextStatics::ShowDamageText(
	UAbilitySystemComponent* TargetASC,
	float Amount,
	ECC_DamageTextStyle Style)
{
	if (!TargetASC || FMath::IsNearlyZero(Amount))
	{
		return;
	}

	FGameplayCueParameters CueParams;
	// 约定：正数为伤害，负数表示治疗。这里统一取正值传递，
	// 具体的正负语义由 Style 决定，避免下游拿到互相矛盾的符号。
	CueParams.RawMagnitude = FMath::Abs(Amount);
	CueParams.Location = ResolveTextLocation(TargetASC);

	switch (Style)
	{
	case ECC_DamageTextStyle::Critical:
		CueParams.AggregatedSourceTags.AddTag(DamageTextCueTags::Critical);
		break;
	case ECC_DamageTextStyle::Heal:
		CueParams.AggregatedSourceTags.AddTag(DamageTextCueTags::Heal);
		break;
	case ECC_DamageTextStyle::Normal:
	default:
		break;
	}

	TargetASC->ExecuteGameplayCue(DamageTextCueTags::HitImpact, CueParams);
}

// 用扣血前后的生命差值显示实际变化；当前逻辑中 bCritical 优先于治疗样式，请勿在治疗时置 true。
void UCC_DamageTextStatics::ShowDamageTextFromDelta(
	UAbilitySystemComponent* TargetASC,
	float HealthBefore,
	float HealthAfter,
	bool bCritical)
{
	if (!TargetASC)
	{
		return;
	}

	const float Delta = HealthBefore - HealthAfter;
	if (FMath::IsNearlyZero(Delta))
	{
		// 差值恰好为 0 说明没有实际生命值变化（例如目标已满血、被免伤规则吃掉）。
		// 这种时候不飘字，否则会出现 0 伤害的数字。
		return;
	}

	ECC_DamageTextStyle Style;
	if (bCritical)
	{
		Style = ECC_DamageTextStyle::Critical;
	}
	else
	{
		// 差值为负表示生命值上升，是治疗。
		Style = Delta < 0.0f ? ECC_DamageTextStyle::Heal : ECC_DamageTextStyle::Normal;
	}

	ShowDamageText(TargetASC, Delta, Style);
}
