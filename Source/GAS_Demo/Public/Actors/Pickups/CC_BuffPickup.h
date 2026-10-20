#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CC_BuffPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UGameplayEffect;

/**
 * 可放入场景的 buff 道具，编辑器中配置拾取后获得的效果类、范围和外观。
 * 服务器确认角色并施加效果，GAS 接受应用后才消耗道具；客户端通过复制显示结果。
 * 道具只负责拾取入口，不保存或实现 buff 的修饰器、触发器和堆叠规则。
 */
UCLASS()

class GAS_DEMO_API ACC_BuffPickup : public AActor
{
    GENERATED_BODY()
  public:
    ACC_BuffPickup();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup") TObjectPtr<USphereComponent> PickupSphere;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup") TSubclassOf<UGameplayEffect> BuffEffect;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1")) float BuffLevel = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1")) float PickupRadius = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup") bool bPlayersOnly = true;
    /**
     * 编辑器放置或修改道具时同步拾取球体范围，属性值使用世界中的厘米单位。
     */
    virtual void OnConstruction(const FTransform& Transform) override;

  protected:
    virtual void BeginPlay() override;

  private:
    bool bConsumed = false;
    /**
     * 出生后下一帧检查已有重叠角色，处理道具生成时角色已在拾取范围内的情况。
     */
    void CheckInitialOverlaps();
    /**
     * 服务器确认拾取角色并施加 BuffEffect；只有 GAS 接受应用才销毁道具。
     * 使用原生应用结果判断成功，兼容 Instant 效果没有活动句柄的情况。
     */
    UFUNCTION()
    void OnPickupOverlap(UPrimitiveComponent* Component, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
                         int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
};
