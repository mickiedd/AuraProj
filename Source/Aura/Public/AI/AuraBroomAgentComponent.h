// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviacAgent.h"
#include "AuraBroomAgentComponent.generated.h"

class AAuraBroomVehicle;

/**
 * UAuraBroomAgentComponent
 *
 * Aura-specific Behaviac agent for the broom vehicle (AAuraBroomVehicle). This
 * component is the BT *glue*: it auto-loads BT_BroomFollowPlayer.xml, seeds the
 * blackboard, and binds the tree's <Action> nodes to C++ methods. It deliberately
 * owns no behavior logic — the follow decision (mount hand-off, post-dismount
 * back-off, approach, coast) lives on the follow component
 * (UAuraBroomFollowComponent::ComputeFollowThrust) and is driven each tick by the
 * flight driver (UAuraBroomFlightDriverComponent). That keeps the vehicle
 * Behaviac-agnostic and this component thin.
 *
 * Bound methods:
 *   - FindPlayer:    locates a player character, stores their location in the
 *                    blackboard (Self.PlayerLocation), sets Self.bPlayerInScene.
 *   - FollowPlayer:  a no-op success; per-tick steering is owned by the flight
 *                    driver (the BT only ticks ~10 Hz, too coarse for smooth
 *                    steering). The BT's role is player discovery + in-scene
 *                    gating via the bPlayerInScene precondition.
 *
 * The methods execute on the game thread (Phase 1 of the Behaviac world
 * subsystem tick), so they may safely touch the owning broom actor.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Broom Behaviac Agent")
class AURA_API UAuraBroomAgentComponent : public UBehaviacAgentComponent
{
	GENERATED_BODY()

public:
	UAuraBroomAgentComponent();

	virtual void BeginPlay() override;

protected:
	// --- Behavior methods bound to BT_BroomFollowPlayer.xml ---

	/** Find a player character in the scene and store their location. */
	EBehaviacStatus Method_FindPlayer();

	/** Delegate the follow thrust/yaw decision to the vehicle and apply it. */
	EBehaviacStatus Method_FollowPlayer();

private:
	/** Convenience accessor for the owning broom. */
	AAuraBroomVehicle* GetBroomOwner() const;
};