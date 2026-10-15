#include "Damage/CC_GameplayEffectContext.h"

/**
 * 检查句柄的反射类型并安全取得项目上下文。
 * @param Handle 待检查的 GameplayEffectContext 句柄。
 * @return 只读项目上下文；空句柄或类型不符返回 nullptr。
 */
const FCC_GameplayEffectContext* FCC_GameplayEffectContext::Get(const FGameplayEffectContextHandle& Handle)
{
    return Handle.Get() && Handle.Get()->GetScriptStruct()->IsChildOf(StaticStruct())
               ? static_cast<const FCC_GameplayEffectContext*>(Handle.Get())
               : nullptr;
}

/**
 * 复用类型检查，取得当次上下文的可写指针。
 * @param Handle 待检查的 GameplayEffectContext 句柄。
 * @return 可写项目上下文；空句柄或类型不符返回 nullptr。
 */
FCC_GameplayEffectContext* FCC_GameplayEffectContext::GetMutable(FGameplayEffectContextHandle& Handle)
{
    return const_cast<FCC_GameplayEffectContext*>(Get(Handle));
}

/**
 * 序列化基类数据、伤害类型及已判定的表现标志，不在客户端重新判定暴击。
 * @param Ar 读取或写入上下文的网络序列化 Archive。
 * @param Map 用于网络对象引用映射的 PackageMap。
 * @param bOutSuccess 输出序列化是否成功，不等同于对象引用已经全部完成映射。
 * @return 对象引用均完成映射时返回 true，否则返回 false。
 */
bool FCC_GameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    bool BaseSuccess = false, TagSuccess = false;
    const bool BaseMapped = FGameplayEffectContext::NetSerialize(Ar, Map, BaseSuccess);
    const bool TagMapped = DamageType.NetSerialize(Ar, Map, TagSuccess);
    bool SourceTagsSuccess = false;
    const bool SourceTagsMapped = SourceTags.NetSerialize(Ar, Map, SourceTagsSuccess);
    uint8 Flags = (bCritical ? 1 : 0) | (bUseHitLocation ? 2 : 0);
    Ar.SerializeBits(&Flags, 2);
    if (Ar.IsLoading())
    {
        bCritical = (Flags & 1) != 0;
        bUseHitLocation = (Flags & 2) != 0;
    }
    // 暴击策略只留在权威端；客户端收到 bCritical 后直接展示，不再进行随机判定。
    bOutSuccess = BaseSuccess && TagSuccess && SourceTagsSuccess && !Ar.IsError();
    return BaseMapped && TagMapped && SourceTagsMapped;
}
