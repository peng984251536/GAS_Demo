#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "Actors/Projectiles/CC_ProjectileFireContext.h"
#include "CC_ArrowProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/** 服务器生成并判定命中的箭；位置复制到客户端，客户端不自行造成伤害。 */
UCLASS()

class GAS_DEMO_API ACC_ArrowProjectile : public AActor
{
    GENERATED_BODY()

  public:
    ACC_ArrowProjectile();

    /**
	 * 在完成生成前写入参数并捕获攻击属性，确保碰撞启动时快照已就绪。
	 * @param InSpeed 本次箭速。
	 * @param InPayload 本次放箭的伤害参数；由箭保存副本。
	 * @return 成功准备服务器伤害快照时返回 true。
	 */
    bool InitializeArrow(float InSpeed, const FCC_ArrowPayload& InPayload);
    /**
	 * 忽略同一齐射的其他箭，避免同位置生成的多支箭互相碰撞。
	 * @param Other 同一次齐射生成的投射物。
	 */
    void IgnoreVolleyArrow(ACC_ArrowProjectile* Other);

    /**
     * 服务器反弹：校验资格与敌我，更新归属、碰撞忽略和速度，并重新快照反弹者属性。
     * 保留原箭 Payload 的技能参数；伤害来源和后续伤害事件归反弹者。每支箭最多反弹一次。
     * 已反弹的箭独立存在，资格标签之后移除或被抑制不会撤回它。
     */
    bool Deflect(APawn* NewInstigator);

  protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** 在游戏视口绘制箭实际使用的球形碰撞；可在投射物蓝图中关闭。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Arrow|Debug")
    bool bDrawCollisionDebug = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    TObjectPtr<UStaticMeshComponent> ArrowMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow")
    TObjectPtr<UProjectileMovementComponent> Movement;

  private:
    /** 阻挡碰撞：世界障碍等非 Pawn 物体。 */
    UFUNCTION()
    void OnArrowHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                    FVector NormalImpulse, const FHitResult& Hit);

    /**
     * 重叠：Pawn 通道。敌对目标结算伤害并销毁；友方（含包围圈里的同伴）直接穿过，
     * 否则远程圈的弓兵射向玩家时，箭会先撞上外圈的近战友军被销毁。
     */
    UFUNCTION()
    void OnArrowOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    /**
     * 结算一次有效碰撞：敌对且有 ASC 时提交伤害快照，然后销毁箭。每支箭只结算一次。
     * @param OtherActor 被命中的 Actor。
     * @param Hit 命中信息。
     */
    void ProcessImpact(AActor* OtherActor, const FHitResult& Hit);

    UPROPERTY(Transient) FCC_ArrowPayload Payload;
    FGameplayEffectSpecHandle DamageSnapshot;
    bool bHitProcessed = false;
    /** 箭自身的运行时状态，防止多个攻击形状或双方攻击把同一支箭无限来回反弹。 */
    bool bDeflected = false;
};
