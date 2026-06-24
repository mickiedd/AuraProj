// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "BehaviacAgent.h"
#include "AuraBroomAgentComponent.generated.h"

class AAuraBroomVehicle;

/**
 * UAuraBroomAgentComponent
 *
 * Aura-specific Behaviac agent for the broom vehicle (AAuraBroomVehicle).
 *
 * Auto-loads BT_BroomFollowPlayer.xml on BeginPlay and registers the C++
 * behavior methods the tree invokes:
 *   - FindPlayer:     locates a player character in the scene, stores their
 *                     location in the blackboard (Self.PlayerLocation), and sets
 *                     Self.bPlayerInScene accordingly.
 *   - FollowPlayer:   reads Self.PlayerLocation and sets a persistent flight
 *                     thrust on the broom (SetBtFlightThrust) to steer toward
 *                     the player. The thrust is re-applied every movement tick
 *                     so the broom keeps flying between the BT's ~5-10 Hz pulses.
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

	/** How fast the broom moves toward the player (flight input scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Broom")
	float FollowSpeedScale;

	/**
	 * Distance (cm) at which the broom stops thrusting toward the player and coasts
	 * to a halt via the movement component's Deceleration. Must be >= the braking
	 * distance from MaxSpeed (v^2 / (2*Deceleration)); with MaxSpeed=1200 and
	 * Deceleration=3200 that is ~225cm, so 300 gives margin. Too small and the
	 * broom overshoots/oscillates around the player.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Broom", meta = (ClampMin = "0.0"))
	float FollowStopRadius = 300.f;

	/** If true, the broom only follows when no character is currently mounted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Broom")
	bool bStopFollowWhenMounted;

protected:
	// --- Behavior methods bound to BT_BroomFollowPlayer.xml ---

	/** Find a player character in the scene and store their location. */
	EBehaviacStatus Method_FindPlayer();

	/** Move the broom toward the stored player location via persistent flight thrust. */
	EBehaviacStatus Method_FollowPlayer();

private:
	/** Convenience accessor for the owning broom. */
	AAuraBroomVehicle* GetBroomOwner() const;
};