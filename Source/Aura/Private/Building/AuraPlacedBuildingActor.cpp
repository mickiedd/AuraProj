// Copyright Druid Mechanics

#include "Building/AuraPlacedBuildingActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

AAuraPlacedBuildingActor::AAuraPlacedBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionProfileName(TEXT("BlockAll"));
}

void AAuraPlacedBuildingActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAuraPlacedBuildingActor, PlacedMesh);
}

void AAuraPlacedBuildingActor::SetPlacedMesh(UStaticMesh* Mesh)
{
	if (!Mesh) return;
	PlacedMesh = Mesh;
	MeshComponent->SetStaticMesh(Mesh);
}

void AAuraPlacedBuildingActor::OnRep_PlacedMesh()
{
	if (PlacedMesh)
	{
		MeshComponent->SetStaticMesh(PlacedMesh);
	}
}
