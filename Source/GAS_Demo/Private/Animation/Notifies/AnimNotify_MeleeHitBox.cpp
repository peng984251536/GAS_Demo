#include "Animation/Notifies/AnimNotify_MeleeHitBox.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTags/CC_Tags.h"
#if WITH_EDITOR
#include "SceneManagement.h"
#endif

// ---------------------------------------------------------------------------
// FCC_MeleeHitShape

bool FCC_MeleeHitShape::ResolveWorldPose(const USceneComponent* MeshComp, FVector& OutLocation, FQuat& OutRotation,
	float& OutScale) const
{
	if (!IsValid(MeshComp))
	{
		return false;
	}

	FTransform Base = MeshComp->GetComponentTransform();
	if (!AttachSocket.IsNone() && MeshComp->DoesSocketExist(AttachSocket))
	{
		Base = MeshComp->GetSocketTransform(AttachSocket, RTS_World);
	}

	// 统一成等比缩放，避免非等比缩放把盒子 / 胶囊扭成检测不了的形状。
	OutScale = FMath::Max(static_cast<float>(Base.GetMaximumAxisScale()), KINDA_SMALL_NUMBER);
	Base.SetScale3D(FVector(OutScale));

	const FTransform World = FTransform(Rotation, Offset) * Base;
	OutLocation = World.GetLocation();
	OutRotation = World.GetRotation();
	return true;
}

FCollisionShape FCC_MeleeHitShape::MakeCollisionShape(float Scale) const
{
	switch (ShapeType)
	{
	case ECC_HitShapeType::Box:
		return FCollisionShape::MakeBox(BoxHalfExtent * Scale);
	case ECC_HitShapeType::Capsule:
		return FCollisionShape::MakeCapsule(Radius * Scale, FMath::Max(CapsuleHalfHeight, Radius) * Scale);
	case ECC_HitShapeType::Sphere:
	default:
		return FCollisionShape::MakeSphere(Radius * Scale);
	}
}

void FCC_MeleeHitShape::DrawDebug(const UWorld* World, const FVector& Location, const FQuat& InRotation, float Scale,
	const FColor& Color, float LifeTime) const
{
#if ENABLE_DRAW_DEBUG
	if (!World)
	{
		return;
	}
	switch (ShapeType)
	{
	case ECC_HitShapeType::Box:
		DrawDebugBox(World, Location, BoxHalfExtent * Scale, InRotation, Color, false, LifeTime, SDPG_Foreground, 1.5f);
		break;
	case ECC_HitShapeType::Capsule:
		DrawDebugCapsule(World, Location, FMath::Max(CapsuleHalfHeight, Radius) * Scale, Radius * Scale, InRotation,
			Color, false, LifeTime, SDPG_Foreground, 1.5f);
		break;
	case ECC_HitShapeType::Sphere:
	default:
		DrawDebugSphere(World, Location, Radius * Scale, 16, Color, false, LifeTime, SDPG_Foreground, 1.5f);
		break;
	}
#endif
}

// ---------------------------------------------------------------------------
// UAnimNotify_MeleeHitBox

UAnimNotify_MeleeHitBox::UAnimNotify_MeleeHitBox()
{
	// 出伤帧影响战斗结果；蒙太奇播到此帧时同步触发更准确（可在 Notify 的 Montage Tick Type 改回 Queued）。
	bIsNativeBranchingPoint = true;
	EventTag = CCTags::Event::Attack;
	// 新放进蒙太奇时自带一个盒子，直接就能在预览里看到并调整。
	Shapes.AddDefaulted();

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(230, 60, 40, 255);
#endif
}

const UAnimNotify_MeleeHitBox* UAnimNotify_MeleeHitBox::FromEventPayload(const FGameplayEventData& Payload)
{
	return Cast<const UAnimNotify_MeleeHitBox>(Payload.OptionalObject.Get());
}

void UAnimNotify_MeleeHitBox::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!IsValid(MeshComp))
	{
		return;
	}

#if ENABLE_DRAW_DEBUG
	// 动画编辑器预览里播放时闪一下判定形状（预览角色没有 ASC，不会发事件）。
	const UWorld* World = MeshComp->GetWorld();
	if (World && World->WorldType == EWorldType::EditorPreview)
	{
		for (const FCC_MeleeHitShape& Shape : Shapes)
		{
			FVector Location;
			FQuat Rotation;
			float Scale = 1.0f;
			if (Shape.ResolveWorldPose(MeshComp, Location, Rotation, Scale))
			{
				Shape.DrawDebug(World, Location, Rotation, Scale, FColor::Red, 0.4f);
			}
		}
	}
#endif

	AActor* Owner = MeshComp->GetOwner();
	if (!IsValid(Owner) || !EventTag.IsValid()
		|| !IsValid(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner)))
	{
		return;
	}

	// 与原蓝图 AN_SendEventToActor 一致：发给自身 ASC，由正在执行的攻击 GA 处理。
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	Payload.OptionalObject = this;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

FString UAnimNotify_MeleeHitBox::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("HitBox x%d"), Shapes.Num());
}

#if WITH_EDITOR
void UAnimNotify_MeleeHitBox::DrawInEditor(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* MeshComp,
	const UAnimSequenceBase* Animation, const FAnimNotifyEvent& NotifyEvent) const
{
	if (!PDI || !IsValid(MeshComp))
	{
		return;
	}

	// 预览窗口用单节点实例播放资源；拿不到当前时间时一律按高亮显示。
	bool bHighlight = true;
	if (const UAnimSingleNodeInstance* Preview = MeshComp->GetSingleNodeInstance())
	{
		bHighlight = FMath::Abs(Preview->GetCurrentTime() - NotifyEvent.GetTriggerTime()) <= EditorHighlightWindow;
	}
	if (!bHighlight && !bAlwaysDrawInEditor)
	{
		return;
	}

	const FLinearColor Color = bHighlight ? FLinearColor(1.0f, 0.1f, 0.05f) : FLinearColor(0.6f, 0.35f, 0.05f);
	const float Thickness = bHighlight ? 1.5f : 0.5f;

	for (const FCC_MeleeHitShape& Shape : Shapes)
	{
		FVector Location;
		FQuat Rotation;
		float Scale = 1.0f;
		if (!Shape.ResolveWorldPose(MeshComp, Location, Rotation, Scale))
		{
			continue;
		}
		const FVector X = Rotation.GetAxisX();
		const FVector Y = Rotation.GetAxisY();
		const FVector Z = Rotation.GetAxisZ();

		switch (Shape.ShapeType)
		{
		case ECC_HitShapeType::Box:
			DrawOrientedWireBox(PDI, Location, X, Y, Z, Shape.BoxHalfExtent * Scale, Color, SDPG_Foreground, Thickness);
			break;
		case ECC_HitShapeType::Capsule:
			DrawWireCapsule(PDI, Location, X, Y, Z, Color, Shape.Radius * Scale,
				FMath::Max(Shape.CapsuleHalfHeight, Shape.Radius) * Scale, 16, SDPG_Foreground, Thickness);
			break;
		case ECC_HitShapeType::Sphere:
		default:
			DrawWireSphere(PDI, Location, Color, Shape.Radius * Scale, 16, SDPG_Foreground, Thickness);
			break;
		}
	}
}
#endif
