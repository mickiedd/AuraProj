// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviorUAgent.h"
#include "AuraCarAgentComponent.generated.h"

class AAuraCar;

/**
 * UAuraCarAgentComponent
 *
 * Aura-specific BehaviorU agent for the open-world car (AAuraCar). Auto-loads
 * BT_AuraCarDrive.xml on server BeginPlay and binds its <Action> nodes to C++
 * methods. The agent is thin BT glue — it picks waypoints and seeds the
 * blackboard; per-tick steering/throttle is owned by UAuraCarDriveComponent,
 * which reads Self.WaypointLocation every server tick for smooth control.
 *
 * Bound methods:
 *   - PickWaypoint:  picks a random reachable point around the car within
 *                    WaypointRadius, stores it in Self.WaypointLocation,
 *                    sets Self.bHasWaypoint = true. Returns Failure if no
 *                    world/nav is available (the drive branch's precondition
 *                    will skip, and the car coasts).
 *   - Drive:         no-op success. Per-tick steering is owned by the drive
 *                    component (the BT only ticks ~10 Hz, too coarse for smooth
 *                    driving). The BT's role is waypoint selection + gating.
 *
 * Server-authoritative: skips tree load on clients (movement replicates via
 * the car's replicated CollisionBox physics). The methods execute on the game
 * thread (Phase 1 of the BehaviorU world subsystem tick).
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Car BehaviorU Agent")
class AURA_API UAuraCarAgentComponent : public UBehaviorUAgentComponent
{
	GENERATED_BODY()

public:
	UAuraCarAgentComponent();

	virtual void BeginPlay() override;

protected:
	// --- Behavior methods bound to BT_AuraCarDrive.xml ---

	/** Pick a random reachable point around the car and store it in the blackboard. */
	EBehaviorUStatus Method_PickWaypoint();

	/** No-op success — per-tick steering is owned by UAuraCarDriveComponent. */
	EBehaviorUStatus Method_Drive();

private:
	/** Convenience accessor for the owning car. */
	AAuraCar* GetCarOwner() const;
};