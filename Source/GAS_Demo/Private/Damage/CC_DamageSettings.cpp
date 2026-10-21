#include "Damage/CC_DamageSettings.h"
#include "AbilitySystem/Effects/Damage/CC_GE_Damage.h"
#include "AbilitySystem/Effects/Damage/CC_GE_Healing.h"

UCC_DamageSettings::UCC_DamageSettings()
{
	DamageEffectClass = UCC_GE_Damage::StaticClass(); // harness:allow R3 只选择 GE 类，Spec 仍由 UCC_DamageStatics 构造
	HealingEffectClass = UCC_GE_Healing::StaticClass();
}

TSubclassOf<UCC_GE_Damage> UCC_DamageSettings::GetDamageEffectClass()
{
	UClass* Class = GetDefault<UCC_DamageSettings>()->DamageEffectClass.LoadSynchronous();
	return Class ? Class : UCC_GE_Damage::StaticClass(); // harness:allow R3 只选择 GE 类，Spec 仍由 UCC_DamageStatics 构造
}

TSubclassOf<UCC_GE_Healing> UCC_DamageSettings::GetHealingEffectClass()
{
	UClass* Class = GetDefault<UCC_DamageSettings>()->HealingEffectClass.LoadSynchronous();
	return Class ? Class : UCC_GE_Healing::StaticClass();
}
