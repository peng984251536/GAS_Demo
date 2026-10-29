#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AnimNotify_MeleeHitBox.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
struct FCollisionShape;
struct FGameplayEventData;

UENUM(BlueprintType)
enum class ECC_HitShapeType : uint8
{
	Sphere,
	Box,
	Capsule,
};

/**
 * 一个攻击判定形状。坐标相对角色的 Mesh 组件（或 Mesh 上的骨骼 / 插槽），
 * 所以蒙太奇预览窗口里看到的位置就是游戏里的位置。
 * 注意 UE 小白人 Mesh 的前方是 +Y、原点在脚底。
 */
USTRUCT(BlueprintType)
struct GAS_DEMO_API FCC_MeleeHitShape
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox")
	ECC_HitShapeType ShapeType = ECC_HitShapeType::Box;

	/** 为空：相对 Mesh 组件；填骨骼或插槽名（如 weapon_r）：跟随该骨骼。找不到时退回 Mesh 组件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox")
	FName AttachSocket = NAME_None;

	/** 在 Mesh / 插槽局部空间内的偏移。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox")
	FVector Offset = FVector(0.0, 120.0, 90.0);

	/** 在 Mesh / 插槽局部空间内的旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox")
	FRotator Rotation = FRotator::ZeroRotator;

	/** 球半径 / 胶囊半径。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox",
		meta=(ClampMin="1", EditCondition="ShapeType != ECC_HitShapeType::Box", EditConditionHides))
	float Radius = 80.0f;

	/** 盒子半尺寸（局部 X / Y / Z）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox",
		meta=(EditCondition="ShapeType == ECC_HitShapeType::Box", EditConditionHides))
	FVector BoxHalfExtent = FVector(80.0, 80.0, 60.0);

	/** 胶囊半高（含两端半球，沿局部 Z 轴），与 UE 胶囊体约定一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox",
		meta=(ClampMin="1", EditCondition="ShapeType == ECC_HitShapeType::Capsule", EditConditionHides))
	float CapsuleHalfHeight = 120.0f;

	/**
	 * 计算形状的世界位置、旋转与缩放。OutScale 取 Mesh（或插槽）变换的最大轴缩放，
	 * 体型放大的怪物判定框随之等比放大；预览窗口里 Mesh 缩放为 1，所见即所得。
	 */
	bool ResolveWorldPose(const USceneComponent* MeshComp, FVector& OutLocation, FQuat& OutRotation, float& OutScale) const;

	FCollisionShape MakeCollisionShape(float Scale) const;

	/** 运行时调试绘制（非 Shipping）。 */
	void DrawDebug(const UWorld* World, const FVector& Location, const FQuat& Rotation, float Scale,
		const FColor& Color, float LifeTime) const;
};

/**
 * 近战攻击判定 Notify，取代蓝图 AN_SendEventToActor。
 *
 * 放在攻击蒙太奇的出伤帧：向自身 ASC 发送 EventTag（默认 CCTags.Event.Attack），
 * 在服务器按 Shapes 完成命中检测，将结果放入 Payload.TargetData，自己放入 OptionalObject。
 * 攻击 GA 收到事件后调用 UCC_Primary::HitBoxOverlapFromEvent(Payload) 读取结果，不再检测第二次。
 * GameplayEvent 只在本机 ASC 分发；客户端仍收到空目标的时序事件，伤害以服务器检测为准。
 *
 * 每个放进蒙太奇的 Notify 都是独立实例，因此不同招式、同一招的不同出伤帧可以各配各的判定框。
 * Notify 是蒙太奇资源的子对象，运行时只读，不保存任何角色的状态。
 *
 * 编辑器：把播放头拖到该 Notify 附近，预览窗口会用红色线框画出判定形状；
 * 勾选 bAlwaysDrawInEditor 则任何时间都以暗橙色显示。
 */
UCLASS(meta=(DisplayName=""))
class GAS_DEMO_API UAnimNotify_MeleeHitBox : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_MeleeHitBox();

	/** 发给自身 ASC 的事件，攻击 GA 用 WaitGameplayEvent 等待同一个标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox")
	FGameplayTag EventTag;

	/** 判定形状，可以多个（如武器挥过的扇面用两三个盒子拼）；命中目标会去重。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox", meta=(TitleProperty="{ShapeType} {AttachSocket}"))
	TArray<FCC_MeleeHitShape> Shapes;

	/** 运行时显示实际用于检测的形状与命中点；不改变编辑器预览。受 cc.Debug.Melee 控制：1 = 只画勾选的通知，2 = 全部画。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HitBox|Debug")
	bool bDrawDebug = false;

	/** 仅服务器收集命中；每个调用使用独立结果，多形状/多组件按 Actor 去重。 */
	FGameplayAbilityTargetDataHandle CollectTargetData(USkeletalMeshComponent* MeshComp) const;

	/** 编辑器预览：播放头在触发时间前后多少秒内高亮显示判定形状。 */
	UPROPERTY(EditAnywhere, Category="HitBox|Editor", meta=(ClampMin="0"))
	float EditorHighlightWindow = 0.15f;

	/** 编辑器预览：任何时间都显示判定形状（暗色），方便对照整段动作调位置。 */
	UPROPERTY(EditAnywhere, Category="HitBox|Editor")
	bool bAlwaysDrawInEditor = false;

	/** 取回发出该事件的 HitBox Notify；事件不是本类发出的返回 nullptr。 */
	static const UAnimNotify_MeleeHitBox* FromEventPayload(const FGameplayEventData& Payload);

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual void DrawInEditor(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp,
		const UAnimSequenceBase* Animation, const FAnimNotifyEvent& NotifyEvent) const override;
#endif
};
