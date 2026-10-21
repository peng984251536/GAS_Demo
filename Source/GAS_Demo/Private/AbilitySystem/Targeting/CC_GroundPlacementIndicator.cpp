#include "AbilitySystem/Targeting/CC_GroundPlacementIndicator.h"

#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
/** 引擎 BasicShapes 圆柱：直径 100、高 100，轴心在中心。 */
constexpr float CylinderMeshSize = 100.f;
/** 扁圆盘的厚度（厘米）。 */
constexpr float DiscThickness = 2.f;
}

ACC_GroundPlacementIndicator::ACC_GroundPlacementIndicator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 贴花沿自身 +X 投射，Pitch -90 让它朝下投到地面。
	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(Root);
	Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Decal->SetVisibility(false);

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCollisionResponseToAllChannels(ECR_Ignore);
	Disc->SetGenerateOverlapEvents(false);
	Disc->CanCharacterStepUpOn = ECB_No;
	Disc->SetCastShadow(false);
	Disc->SetReceivesDecals(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CylinderMesh.Succeeded())
	{
		Disc->SetStaticMesh(CylinderMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Disc->SetMaterial(0, ShapeMaterial.Object);
	}
}

void ACC_GroundPlacementIndicator::BeginPlay()
{
	Super::BeginPlay();

	// 指示器只在本地存在，动态材质实例属于这个 Actor，不会写回共享材质资产。
	if (DecalMaterial)
	{
		ColorMaterial = UMaterialInstanceDynamic::Create(DecalMaterial, this);
		Decal->SetDecalMaterial(ColorMaterial);
		Decal->SetVisibility(true);
		Disc->SetVisibility(false);
	}
	else
	{
		ColorMaterial = Disc->CreateDynamicMaterialInstance(0);
		Decal->SetVisibility(false);
		Disc->SetVisibility(true);
	}
	ApplyRadius();
	ApplyColor(false);
}

void ACC_GroundPlacementIndicator::SetRadius(float InRadius)
{
	Radius = FMath::Max(1.f, InRadius);
	ApplyRadius();
}

void ACC_GroundPlacementIndicator::UpdatePlacement(const FVector& Location, bool bValid)
{
	SetActorLocation(Location);
	if (!bHasValidity || bValid != bLastValid)
	{
		bHasValidity = true;
		bLastValid = bValid;
		ApplyColor(bValid);
		OnPlacementValidityChanged(bValid);
	}
}

void ACC_GroundPlacementIndicator::ApplyRadius()
{
	// DecalSize：X 为投射深度的一半，Y/Z 为半径。
	Decal->DecalSize = FVector(DecalDepth * 0.5f, Radius, Radius);
	Decal->MarkRenderStateDirty();

	const float Scale = Radius * 2.f / CylinderMeshSize;
	Disc->SetRelativeScale3D(FVector(Scale, Scale, DiscThickness / CylinderMeshSize));
	Disc->SetRelativeLocation(FVector(0.f, 0.f, DiscHeightOffset + DiscThickness * 0.5f));
}

void ACC_GroundPlacementIndicator::ApplyColor(bool bValid)
{
	if (ColorMaterial)
	{
		ColorMaterial->SetVectorParameterValue(ColorParameterName, bValid ? ValidColor : InvalidColor);
	}
}
