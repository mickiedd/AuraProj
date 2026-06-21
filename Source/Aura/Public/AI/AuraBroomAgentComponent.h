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
 *   - FollowPlayer:   reads Self.PlayerLocation and calls AddFlightInput on the
 *                     broom to steer toward the player.
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

	/** If true, the broom only follows when no character is currently mounted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Broom")
	bool bStopFollowWhenMounted;

protected:
	// --- Behavior methods bound to BT_BroomFollowPlayer.xml ---

	/** Find a player character in the scene and store their location. */
	EBehaviacStatus Method_FindPlayer();

	/** Move the broom toward the stored player location via AddFlightInput. */
	EBehaviacStatus Method_FollowPlayer();

private:
	/** Convenience accessor for the owning broom. */
	AAuraBroomVehicle* GetBroomOwner() const;
};