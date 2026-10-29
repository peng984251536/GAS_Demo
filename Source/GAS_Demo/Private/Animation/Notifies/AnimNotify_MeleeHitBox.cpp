#include "Animation/Notifies/AnimNotify_MeleeHitBox.h"
#include "Debug/CC_DebugCVars.h"
#include "Actors/Projectiles/CC_ProjectileInteractions.h"
#include "Actors/Character/CC_BaseCharacter.h"
#include "Utils/Team/CC_TeamStatics.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
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

FGameplayAbilityTargetDataHandle UAnimNotify_MeleeHitBox::CollectTargetData(USkeletalMeshComponent* MeshComp) const
{
	FGameplayAbilityTargetDataHandle Result;
	AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	UWorld* World = IsValid(MeshComp) ? MeshComp->GetWorld() : nullptr;
	if (!IsValid(Owner) || !Owner->HasAuthority() || !World || !World->IsGameWorld())
	{
		return Result;
	}

	FCollisionQueryParams Query(SCENE_QUERY_STAT(MeleeNotifyHitBox), false, Owner);
	FCollisionObjectQueryParams Objects(ECC_Pawn);
	TSet<AActor*> AddedActors;
	for (const FCC_MeleeHitShape& Shape : Shapes)
	{
		FVector Location;
		FQuat Rotation;
		float Scale = 1.f;
		if (!Shape.ResolveWorldPose(MeshComp, Location, Rotation, Scale)) continue;
		const FCollisionShape Collision = Shape.MakeCollisionShape(Scale);
		// 箭单独处理，不混入角色伤害目标；只使用同一套 Notify 形状。
		CCProjectileInteractions::DeflectInShape(Owner, Location, Rotation, Collision);
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, Location, Rotation, Objects, Collision, Query);
		if (CCDebug::DrawMelee(bDrawDebug)) Shape.DrawDebug(World, Location, Rotation, Scale, FColor::Red, 0.4f);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Target = Overlap.GetActor();
			UPrimitiveComponent* Component = Overlap.GetComponent();
			if (!IsValid(Target) || Target == Owner || !IsValid(Component) || AddedActors.Contains(Target)) continue;
			if (const ACC_BaseCharacter* Character = Cast<ACC_BaseCharacter>(Target))
			{
				if (!Character->IsAlive() || !UCC_TeamStatics::IsHostile(Owner, Target)) continue;
			}

			// Overlap 没有表面命中信息，补一条组件射线供受击方向/命中特效使用。
			const FVector Center = Component->Bounds.Origin;
			FVector Normal = (Owner->GetActorLocation() - Center).GetSafeNormal();
			if (Normal.IsNearlyZero()) Normal = -Owner->GetActorForwardVector();
			const float Distance = Component->Bounds.SphereRadius + 10.f;
			FHitResult Hit;
			if (!Component->LineTraceComponent(Hit, Center + Normal * Distance, Center - Normal * Distance, Query)
				|| Hit.bStartPenetrating)
			{
				// 检测已经确认重叠，补射线失败不能把实际命中丢掉。
				FVector Point = Center;
				Component->GetClosestPointOnCollision(Owner->GetActorLocation(), Point);
				Hit = FHitResult(Target, Component, Point, Normal);
			}
			Result.Add(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
			AddedActors.Add(Target);
#if ENABLE_DRAW_DEBUG
			if (CCDebug::DrawMelee(bDrawDebug)) DrawDebugPoint(World, Hit.ImpactPoint, 10.f, FColor::Green, false, 0.4f);
#endif
		}
	}
	return Result;
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

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = Owner;
	Payload.Target = Owner;
	Payload.OptionalObject = this;
	Payload.TargetData = CollectTargetData(MeshComp);
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
