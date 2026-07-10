// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/AuraBroomMovement.h"
#include "Vehicle/AuraBroomMotionComponent.h"
#include "Vehicle/AuraBroomMountComponent.h"
#include "Vehicle/AuraBroomFollowComponent.h"
#include "AuraBroomVehicle.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPrimitiveComponent;
class ACharacter;
class UAuraBroomAgentComponent;
class UAuraBroomFlightDriverComponent;
struct FHitResult;

UCLASS(Blueprintable)
class AURA_API AAuraBroomVehicle : public APawn
{
	GENERATED_BODY()

public:
	AAuraBroomVehicle();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Broom|Movement")
	void AddFlightInput(const FVector& WorldDirection, float ScaleValue = 1.f);

	/**
	 * Sets the persistent world-space flight thrust the BT uses to follow a player.
	 * Unlike AddFlightInput (one-shot, consumed each tick), this thrust is re-applied
	 * every movement tick so the broom continuously accelerates between the BT's
	 * ~5-10 Hz pulses. Pass FVector::ZeroVector to stop (coast/decelerate).
	 * Forwards to the follow component, which owns the thrust state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Broom|Movement")
	void SetBtFlightThrust(const FVector& WorldThrust) { if (FollowComponent) FollowComponent->SetFlightThrust(WorldThrust); }

	UFUNCTION(BlueprintPure, Category = "Broom|Movement")
	FVector GetBtFlightThrust() const { return FollowComponent ? FollowComponent->GetFlightThrust() : FVector::ZeroVector; }

	/**
	 * Returns the minimum Z the broom is allowed to fly at during autonomous
	 * follow (player.Z + NeverDescendBelowPlayerOffset), used by the movement
	 * component as a hard position floor so the broom can never end up below the
	 * player. Forwards to the flight driver, which owns the cached player location.
	 * Returns false (no floor) when a rider is mounted — the rider may dive freely —
	 * or when no player location is known.
	 */
	bool GetMinFlightZ(float& OutFloorZ) const;

	/** Accessor for the server-authoritative flight movement component. */
	UFUNCTION(BlueprintPure, Category = "Broom|Movement")
	UAuraBroomMovement* GetFlightMovement() const { return FlightMovement; }

	/** Accessor for the autonomous-flight driver (per-tick thrust + yaw + diagnostics). */
	UFUNCTION(BlueprintPure, Category = "Broom|Components")
	UAuraBroomFlightDriverComponent* GetFlightDriver() const { return FlightDriverComponent; }

	/** Accessor for the follow policy component (follow decision + thrust state). */
	UFUNCTION(BlueprintPure, Category = "Broom|Components")
	UAuraBroomFollowComponent* GetFollowComponent() const { return FollowComponent; }

	/** Throttle period (s) shared by the broom's movement/flight debug logs. */
	float GetMovementDebugLogInterval() const { return FlightInputLogInterval; }

	/**
	 * True while the broom is actively flying — non-zero velocity, pending movement
	 * input, or a recent AddFlightInput within the yield window. The motion
	 * component uses this to decide whether to yield the idle hover to flight.
	 */
	bool IsFlightInputActive() const;

	/**
	 * Sets the yaw the broom will smoothly rotate toward during flight.
	 * Called by the rider's movement input (A/D/S) and by the flight driver (which
	 * derives it from the follow direction) to keep both systems in sync and
	 * prevent them from fighting each other.
	 */
	UFUNCTION(BlueprintCallable, Category = "Broom|Movement")
	void SetFlightTargetYaw(float WorldYaw);

	/** Accessor for the broom mesh (used by the mount component for socket attach/dismount). */
	UFUNCTION(BlueprintPure, Category = "Broom|Components")
	UStaticMeshComponent* GetBroomMesh() const { return BroomMesh; }

	UFUNCTION(BlueprintCallable, Category = "Broom|Mount")
	void RequestMount(ACharacter* CharacterToMount);

	UFUNCTION(BlueprintCallable, Category = "Broom|Mount")
	void RequestDismount();

	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	ACharacter* GetMountedCharacter() const { return MountComponent ? MountComponent->GetMountedCharacter() : nullptr; }

	/** The character that most recently dismounted, or nullptr if none/it's stale. */
	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	ACharacter* GetLastDismountedCharacter() const { return MountComponent ? MountComponent->GetLastDismountedCharacter() : nullptr; }

	/**
	 * Seconds elapsed since the most recent dismount, or -1.f if no dismount has
	 * been recorded. Used by the BT agent to drive a short "back away from the
	 * player" window after a rider dismounts so they don't immediately collide
	 * with the broom and remount by accident.
	 */
	UFUNCTION(BlueprintPure, Category = "Broom|Mount")
	float GetTimeSinceLastDismount() const { return MountComponent ? MountComponent->GetTimeSinceLastDismount() : -1.f; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UStaticMeshComponent> BroomMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UAuraBroomMovement> FlightMovement;

	/** Idle-hover + flight-yaw smoothing. Driven from the broom's Tick via TickMotion. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UAuraBroomMotionComponent> MotionComponent;

	/** Mount/dismount state machine + collision-driven mounting. Replicated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UAuraBroomMountComponent> MountComponent;

	/** Autonomous BT follow policy + persistent follow thrust. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UAuraBroomFollowComponent> FollowComponent;

	/** Drives the broom's autonomous flight each server tick (thrust + yaw + diagnostics). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|Components")
	TObjectPtr<UAuraBroomFlightDriverComponent> FlightDriverComponent;

	/** BehaviorU (BehaviorU) agent driving the broom follow behavior tree (BT_BroomFollowPlayer.xml). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Broom|AI")
	TObjectPtr<UAuraBroomAgentComponent> BroomAgentComponent;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Debug|Movement", meta = (ClampMin = "0.1"))
	float FlightInputLogInterval = 0.25f;

	float LastFlightInputLogTime = -1000.f;
	float LastFlightBlockedLogTime = -1000.f;
	// Server time of the most recent AddFlightInput call, so the motion component's
	// idle hover can keep yielding to flight between BT input pulses (BT fires ~10 Hz).
	float LastFlightInputAppliedTime = -1000.f;
};
