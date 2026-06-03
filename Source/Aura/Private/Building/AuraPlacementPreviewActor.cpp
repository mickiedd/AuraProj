// Copyright Druid Mechanics

#include "Building/AuraPlacementPreviewActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AAuraPlacementPreviewActor::AAuraPlacementPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	bNetLoadOnClient = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
}

void AAuraPlacementPreviewActor::SetPreviewMesh(UStaticMesh* Mesh)
{
	if (!Mesh || !MeshComponent) return;

	MeshComponent->SetStaticMesh(Mesh);

	const int32 NumMats = MeshComponent->GetNumMaterials();
	OriginalMaterials.SetNum(NumMats);
	for (int32 i = 0; i < NumMats; ++i)
	{
		OriginalMaterials[i] = MeshComponent->GetMaterial(i);
	}
}

void AAuraPlacementPreviewActor::SetPlacementValid(bool bValid)
{
	if (bCurrentlyValid == bValid) return;
	bCurrentlyValid = bValid;

	UMaterialInterface* Override = bValid ? ValidMaterial.Get() : InvalidMaterial.Get();
	const int32 NumMats = MeshComponent->GetNumMaterials();
	for (int32 i = 0; i < NumMats; ++i)
	{
		UMaterialInterface* Mat = Override ? Override
			: (OriginalMaterials.IsValidIndex(i) ? OriginalMaterials[i].Get() : nullptr);
		MeshComponent->SetMaterial(i, Mat);
	}
}
