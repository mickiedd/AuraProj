// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraCar.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UBoxComponent;
class UAuraCarAgentComponent;
class UAuraCarDriveComponent;

/**
 * Open-world car actor. For now this is just a thin wrapper around one of the
 * Cartoon City car static meshes (SM_Car_06 / 13 / 16 / 19) plus its four wheel
 * meshes, so the level can host drivable car actors in place of the bare static
 * meshes currently placed in the open-world map. Driving behavior will be
 * layered onto this later.
 *
 * Hierarchy: CollisionBox (root, drives physics/collision) -> CarMesh (visual,
 * NoCollision) -> four wheels (NoCollision). Keeping collision on a simple box
 * decouples the hitbox from the mesh's complex convex collision and gives the
 * driving code a stable, tunable primitive to move. Tune CollisionBox extent
 * per car in a BP subclass.
 *
 * The Cartoon City pack authors each wheel mesh with its pivot at the car
 * body's origin, so the four wheel components are attached at identity
 * transform and reconstruct the assembled car. Adjust WheelXxx relative
 * transforms in a BP subclass only if a mesh turns out to differ.
 */
UCLASS(Blueprintable)
class AURA_API AAuraCar : public AActor
{
	GENERATED_BODY()

public:
	AAuraCar();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;

	/** Returns the root collision box that drives the car's physics/collision. */
	UFUNCTION(BlueprintPure, Category = "Car|Components")
	UBoxComponent* GetCollisionBox() const { return CollisionBox; }

	/** Returns the car body mesh component. */
	UFUNCTION(BlueprintPure, Category = "Car|Components")
	UStaticMeshComponent* GetCarMesh() const { return CarMesh; }

	UFUNCTION(BlueprintPure, Category = "Car|Components")
	UStaticMeshComponent* GetWheelMeshFrontLeft() const { return WheelFrontLeft; }

	UFUNCTION(BlueprintPure, Category = "Car|Components")
	UStaticMeshComponent* GetWheelMeshFrontRight() const { return WheelFrontRight; }

	UFUNCTION(BlueprintPure, Category = "Car|Components")
	UStaticMeshComponent* GetWheelMeshRearLeft() const { return WheelRearLeft; }

	UFUNCTION(BlueprintPure, Category = "Car|Components")
	UStaticMeshComponent* GetWheelMeshRearRight() const { return WheelRearRight; }

	/** Returns the Behaviac agent component (BT waypoint selection + blackboard). */
	UFUNCTION(BlueprintPure, Category = "Car|AI")
	UAuraCarAgentComponent* GetCarAgentComponent() const { return CarAgentComponent; }

	/** Returns the per-tick physics drive component (steering + throttle). */
	UFUNCTION(BlueprintPure, Category = "Car|AI")
	UAuraCarDriveComponent* GetCarDriveComponent() const { return CarDriveComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|Components")
	TObjectPtr<UStaticMeshComponent> CarMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|Components")
	TObjectPtr<UStaticMeshComponent> WheelFrontLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|Components")
	TObjectPtr<UStaticMeshComponent> WheelFrontRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|Components")
	TObjectPtr<UStaticMeshComponent> WheelRearLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|Components")
	TObjectPtr<UStaticMeshComponent> WheelRearRight;

	/** Body mesh applied to CarMesh. Defaults to SM_Car_06; swap to 13/16/19 per instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Car|Appearance")
	TObjectPtr<UStaticMesh> DefaultCarMesh;

	/** Wheel meshes. Default to the SM_Car_06 wheels; swap to 13/16/19 per instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Car|Appearance")
	TObjectPtr<UStaticMesh> DefaultWheelFrontLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Car|Appearance")
	TObjectPtr<UStaticMesh> DefaultWheelFrontRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Car|Appearance")
	TObjectPtr<UStaticMesh> DefaultWheelRearLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Car|Appearance")
	TObjectPtr<UStaticMesh> DefaultWheelRearRight;

	/** BehaviorU (Behaviac) agent driving the autonomous car behavior tree (BT_AuraCarDrive.xml). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|AI")
	TObjectPtr<UAuraCarAgentComponent> CarAgentComponent;

	/** Per-tick physics driver: reads the BT waypoint and applies throttle + steering. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Car|AI")
	TObjectPtr<UAuraCarDriveComponent> CarDriveComponent;
};