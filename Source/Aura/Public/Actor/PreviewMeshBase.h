// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PreviewMeshBase.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Base class for the placement preview ghost actor used by the building system.
 * BP_PreviewMesh inherits from this and can author its own StaticMesh component
 * in the Blueprint editor — this C++ base provides the shared interface.
 */
UCLASS()
class AURA_API APreviewMeshBase : public AActor
{
	GENERATED_BODY()
	
public:
	APreviewMeshBase();

	/** Set which static mesh the preview should display. */
	UFUNCTION(BlueprintCallable, Category = "Building|Preview")
	void SetPreviewMesh(UStaticMesh* Mesh);

	/**
	 * Toggle valid/invalid visual state.
	 * When invalid the mesh is hidden entirely.
	 * When valid the mesh is shown with its original materials (or ValidMaterial override).
	 */
	UFUNCTION(BlueprintCallable, Category = "Building|Preview")
	void SetPlacementValid(bool bValid);

	/** The mesh component driven by this preview actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Building|Preview")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Optional override applied to all material slots when placement is valid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Preview")
	TObjectPtr<UMaterialInterface> ValidMaterial;

	/** Optional override applied to all material slots when placement is invalid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building|Preview")
	TObjectPtr<UMaterialInterface> InvalidMaterial;

private:
	bool bCurrentlyValid = true;
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
};
