// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraPlacementPreviewActor.generated.h"

class UStaticMeshComponent;
class UStaticMesh;

/**
 * Client-only ghost actor that shows a placement preview with valid/invalid visual feedback.
 * Spawned by UAuraBuildingComponent. Has no collision and does not replicate.
 */
UCLASS()
class AURA_API AAuraPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AAuraPlacementPreviewActor();

	/** Set the mesh to display for this placement preview. */
	void SetPreviewMesh(UStaticMesh* Mesh);

	/**
	 * Update the visual feedback for placement validity.
	 * If ValidMaterial / InvalidMaterial are assigned they will be swapped in.
	 * Otherwise the mesh keeps its original materials (indication via on-screen message only).
	 */
	void SetPlacementValid(bool bValid);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Optional override material applied to all slots when placement is valid. */
	UPROPERTY(EditAnywhere, Category = "Building")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	/** Optional override material applied to all slots when placement is invalid. */
	UPROPERTY(EditAnywhere, Category = "Building")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

private:
	bool bCurrentlyValid = true;
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
};
