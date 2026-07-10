// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraCarDriveComponent.generated.h"

class AAuraCar;
class UBoxComponent;

/**
 * UAuraCarDriveComponent
 *
 * Per-tick kinematic driver for the autonomous car. Reads the current waypoint
 * from the BehaviorU agent's blackboard (Self.WaypointLocation) every server
 * tick and moves the car toward it via SetActorLocationAndRotation (no physics).
 * This is the car equivalent of the broom's flight driver: the BT only ticks at
 * ~10 Hz (waypoint selection + gating), while this component runs at 60+ Hz for
 * smooth control.
 *
 * Server-authoritative: disabled on clients in BeginPlay; the car's transform
 * replicates to clients via ReplicatedMovement.
 *
 * Movement model: each tick the component computes the yaw delta between the
 * car's forward vector and the direction to the waypoint, clamps it to
 * MaxSteerAngle, and rotates the car by TurnRate * DeltaTime (capped by the
 * yaw delta so it doesn't overshoot). The car then moves forward along its
 * (new) forward vector by CurrentSpeed * DeltaTime, with arrival easing that
 * slows the car as it approaches the waypoint. When within ArrivalRadius, the
 * component clears bHasWaypoint so the BT re-picks.
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Car Drive")
class AURA_API UAuraCarDriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraCarDriveComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Drive tunables ---

	/** Cruise speed (cm/s) the car drives at toward the waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Drive", meta = (ClampMin = "0.0"))
	float MaxSpeed = 1200.f;

	/** Turn rate (deg/s) — how fast the car rotates toward the waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Drive", meta = (ClampMin = "0.0"))
	float TurnRate = 90.f;

	/** Max steering angle (degrees) — clamps the yaw delta per tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Drive", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxSteerAngle = 45.f;

	/** Distance (cm) at which the car considers the waypoint reached and stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Drive", meta = (ClampMin = "0.0"))
	float ArrivalRadius = 300.f;

	/** Distance (cm) over which speed eases to zero as the car approaches the waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Drive", meta = (ClampMin = "0.0"))
	float ArrivalEaseRange = 600.f;

	/** If true, driving only runs on the authority (server). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Car|Drive")
	bool bServerAuthoritative = true;

private:
	/** Convenience accessor for the owning car. */
	AAuraCar* GetCarOwner() const;

	/** Throttle for the diagnostic drive log. */
	float LastDriveLogTime = -1000.f;
	static constexpr float DriveLogInterval = 0.25f;
};