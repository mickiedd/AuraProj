// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CollisionQueryParams.h"
#include "AuraBuildingComponent.generated.h"

class APreviewMeshBase;
class AAuraPlacedBuildingActor;
class UStaticMesh;

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	Idle    UMETA(DisplayName = "Idle"),
	Placing UMETA(DisplayName = "Placing")
};

/**
 * Manages the UGC placement workflow for the owning AuraCharacter.
 *
 * Lifecycle:
 *   EnterPlacementMode(Mesh)  ->  preview follows character, validity shown on-screen
 *   ConfirmPlacement()        ->  sends Server RPC to spawn authoritative placed actor
 *   CancelPlacement()         ->  destroys preview, returns to Idle
 *
 * Console commands (via AuraPlayerController Exec):
 *   StartPlacement  <full asset path>   e.g.  /Game/Cartoon_City_Free/Meshes/Roads/SM_road_001.SM_road_001
 *   ConfirmPlacement
 *   CancelPlacement
 *   RotatePlacement <degrees>
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AURA_API UAuraBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraBuildingComponent();
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Enter placement mode with the given static mesh as the preview. Client-side only. */
	void EnterPlacementMode(UStaticMesh* Mesh);

	/** Validate current placement and send a server RPC to spawn the placed actor. */
	void ConfirmPlacement();

	/** Cancel placement mode and destroy the preview. */
	void CancelPlacement();

	/** Rotate the pending placement by DeltaYaw degrees around Z. */
	void RotatePlacement(float DeltaYaw);

	UFUNCTION(BlueprintPure, Category = "Building")
	EBuildingState GetBuildingState() const { return BuildingState; }

	// ---- Designer-facing settings ----------------------------------------

	/** Snap placement position to this grid size in cm. Set to 0 to disable snapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float GridSnapSize = 200.f;

	/** Distance in front of the character where the preview appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
	float PlacementDistance = 400.f;

	/**
	 * Blueprint subclass of APreviewMeshBase to spawn as the placement ghost.
	 * Defaults to BP_PreviewMesh. Leave empty to fall back to the bare C++ class.
	 */
	UPROPERTY(EditAnywhere, Category = "Building")
	TSubclassOf<APreviewMeshBase> PlacementPreviewClass;

	/**
	 * Blueprint subclass of AAuraPlacedBuildingActor to spawn on the server.
	 * Leave empty to use the base C++ class.
	 */
	UPROPERTY(EditAnywhere, Category = "Building")
	TSubclassOf<AAuraPlacedBuildingActor> PlacedActorClass;

private:
	EBuildingState BuildingState = EBuildingState::Idle;

	UPROPERTY()
	TObjectPtr<APreviewMeshBase> PreviewActor;

	UPROPERTY()
	TObjectPtr<UStaticMesh> PendingMesh;

	float PendingYaw = 0.f;
	float PersistentPlacementYaw = 0.f;
	bool bLastValidState = false;
	bool bHasLastValidState = false;
	bool bHasGroundAltitudeOverride = false;
	float CachedGroundAltitude = 0.f;

	// ---- Helpers ----------------------------------------------------------

	/** Load level-specific placement altitude from Content/Config/LevelConfig.json. */
	void CacheGroundAltitudeFromLevelConfig();

	/** World-space transform for the current placement position. */
	FTransform CalculatePlacementTransform() const;

	/** Returns false if any WorldStatic object overlaps the placement bounds. */
	bool IsPlacementValid(const FTransform& PlacementTransform) const;

	/** Returns false if the placement location cannot be projected onto the NavMesh. */
	bool IsLocationReachable(const FVector& Location) const;

	FVector SnapToGrid(const FVector& Location) const;

	// ---- RPCs -------------------------------------------------------------

	/** Server: validate and spawn the authoritative placed actor. */
	UFUNCTION(Server, Reliable)
	void ServerRequestPlacement(UStaticMesh* Mesh, FTransform Transform);
};
