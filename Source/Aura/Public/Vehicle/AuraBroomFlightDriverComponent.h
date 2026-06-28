// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraBroomFlightDriverComponent.generated.h"

class AAuraBroomVehicle;

/**
 * UAuraBroomFlightDriverComponent
 *
 * Drives the broom's autonomous flight each server tick: resolves the player's
 * live location, runs the follow policy (UAuraBroomFollowComponent::ComputeFollowThrust),
 * applies the resulting thrust + yaw target, and feeds the thrust into the movement
 * component as standard input. Also owns the flight smoothness diagnostic log and the
 * cached player location that backs the movement component's altitude floor
 * (GetMinFlightZ).
 *
 * Extracted from UAuraBroomMovement so the movement component is responsible only
 * for movement integration (+ enforcing the floor), not for orchestrating the follow
 * system. Ticks before the movement component (created earlier in the broom
 * constructor) so its AddInputVector is consumed in the same movement tick.
 *
 * Server-authoritative: ticks only on the authority (disabled on clients in
 * BeginPlay); movement is replicated to clients. When a rider is mounted the follow
 * policy returns zero thrust, so this component adds nothing and the rider's own
 * AddFlightInput drives the broom.
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Broom Flight Driver")
class AURA_API UAuraBroomFlightDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraBroomFlightDriverComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Returns the minimum Z the broom is allowed to fly at during autonomous follow
	 * (player.Z + NeverDescendBelowPlayerOffset), used by the movement component as a
	 * hard position floor so the broom can never end up below the player. Returns false
	 * (no floor) when a rider is mounted (the rider may dive freely) or when no player
	 * location is known.
	 */
	bool GetMinFlightZ(float& OutFloorZ) const;

private:
	/** Convenience accessor for the owning broom. */
	AAuraBroomVehicle* GetBroomOwner() const;

	// Last player location resolved this server tick, cached so the movement
	// component's Z-floor clamp can use it without a second GetPlayerCharacter
	// lookup. Valid only while bHasValidPlayerTarget is true.
	FVector LastKnownPlayerLocation = FVector::ZeroVector;
	bool bHasValidPlayerTarget = false;

	// Throttle for the [BroomFollow] diag smoothness log (reuses FlightInputLogInterval
	// on the broom as the period).
	float LastFollowDiagLogTime = -1000.f;
};