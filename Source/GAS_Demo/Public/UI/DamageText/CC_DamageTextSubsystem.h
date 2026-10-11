#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/DamageText/CC_DamageTextTypes.h"
#include "CC_DamageTextSubsystem.generated.h"
/** 本地飘字管理器：按玩家/目标保存条目、可选合并、限制数量和回收；不计算战斗伤害。 */
UCLASS()
class GAS_DEMO_API UCC_DamageTextSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    /**
     * 判断当前世界是否需要本地飘字子系统；专用服务器不创建。
     * @param Outer 子系统所属对象，通常是当前 UWorld。
     * @return 允许创建返回 true，否则返回 false。
     */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    /**
     * 更新数字年龄与合并脉冲，回收过期条目，并通知画布重绘。
     * @param DeltaTime 本次更新经过的秒数，用于年龄和脉冲衰减。
     */
    virtual void Tick(float DeltaTime) override;
    /**
     * 提供子系统更新开销的性能统计标识。
     * @return 供引擎 stat 系统使用的统计标识。
     */
    virtual TStatId GetStatId() const override;
    /**
     * 没有活动数字时停止更新，避免空闲帧持续查询。
     * @return 子系统可运行且存在活动条目时返回 true。
     */
    virtual bool IsTickable() const override;
    /**
     * 接收 Cue 的本地表现：可选同组累加，按玩家限制数量，再按目标安排初始错位。
     * @param Viewer 本次数字的本地观看者；必须是有效的本地 PlayerController。
     * @param Target 受击或被治疗的目标，用于归属与合并分组。
     * @param Source 攻击或治疗来源，用于归属与合并分组，可为空。
     * @param Location 数字起点的世界坐标；后续移动只影响投影，不自动跟随目标。
     * @param Amount 指定数值；ReportHitForPlayer 中正数表示伤害、负数表示治疗。
     * @param Style 普通、暴击或治疗样式，决定颜色、字号及治疗加号。
     * @param DamageType 伤害分类标签；不同分类不合并，当前不自动计算元素抗性。
     */
    void ReportHitForPlayer(APlayerController* Viewer, AActor* Target, AActor* Source, const FVector& Location,
        float Amount, ECC_DamageTextStyle Style, FGameplayTag DamageType);
    /** 条目变化通知，容器据此复用、更新和回收 UMG。 */
    FSimpleMulticastDelegate OnEntriesUpdated;
    /**
     * 登记玩家的飘字绘制层，用于发现 RootLayout 与 HUD 重复挂载。
     * @param Player 当前本地玩家，用于分屏归属和绘制层登记。
     * @return 该玩家登记后的绘制层数量；大于 1 表示可能重复绘制。
     */
    int32 AddDrawLayer(const APlayerController* Player);
    /**
     * 撤销玩家的绘制层登记。
     * @param Player 当前本地玩家，用于分屏归属和绘制层登记。
     */
    void RemoveDrawLayer(const APlayerController* Player);
    /**
     * 取得唯一的只读活动列表；绘制时按 Viewer 筛选，不复制展示数据。
     * @return 当前活动条目的只读引用，不要跨帧保存元素引用。
     */
    const TArray<FCC_DamageTextEntry>& GetActiveEntries() const { return ActiveEntries; }
    /** 合并窗口秒数，0=逐击显示；只有同一完整分组允许合并。 */
    UPROPERTY(EditAnywhere, Category="Damage Text", meta=(ClampMin="0")) float MergeWindow = 0.f;
    /** 同组数字允许合并的世界距离阈值（厘米），不能代替目标身份。 */
    UPROPERTY(EditAnywhere, Category="Damage Text", meta=(ClampMin="0")) float MergeDistance = 120.f;
    /** 数字的存活时长，合并时重新计时。 */
    UPROPERTY(EditAnywhere, Category="Damage Text", meta=(ClampMin="0.1")) float Lifetime = 1.1f;
    /** 每玩家条目上限，满时优先淘汰普通伤害，数字丢弃不影响结算。 */
    UPROPERTY(EditAnywhere, Category="Damage Text", meta=(ClampMin="1")) int32 MaxEntriesPerPlayer = 128;
    /** true 显示所有角色的数字，false 仅显示自己造成或受到的数字。 */
    UPROPERTY(EditAnywhere, Category="Damage Text") bool bShowAllDamage = false;
    /** 调试值：最近提交是否合并，0=新建、1=合并。 */
    int32 LastMergeCount = 0;
private:
    /** 按本地玩家记录绘制层数量，只用于重复显示诊断。 */
    TMap<TWeakObjectPtr<const APlayerController>, int32> DrawLayerCounts;
    /** 多个本地玩家共用的活动条目，每条记录独立 Viewer。 */
    TArray<FCC_DamageTextEntry> ActiveEntries;
    /** 只在本地生成，不作为伤害 RPC 或服务器结算 ID。 */
    uint64 NextEntryId = 1;
};
