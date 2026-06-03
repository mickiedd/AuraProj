// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraPlacedBuildingActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * Server-authoritative actor that represents a permanently placed building piece.
 * Spawned by UAuraBuildingComponent::ServerRequestPlacement and replicated to all clients.
 * The static mesh is replicated via OnRep so late-joining clients see the correct mesh.
 */
UCLASS()
class AURA_API AAuraPlacedBuildingActor : public AActor
{
	GENERATED_BODY()

public:
	AAuraPlacedBuildingActor();

	/** Called on the server immediately after spawn to set (and replicate) the mesh. */
	UFUNCTION(BlueprintCallable, Category = "Building")
	void SetPlacedMesh(UStaticMesh* Mesh);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_PlacedMesh)
	TObjectPtr<UStaticMesh> PlacedMesh;

	UFUNCTION()
	void OnRep_PlacedMesh();
};
