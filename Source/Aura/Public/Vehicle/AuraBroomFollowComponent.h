// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AuraBroomFollowComponent.generated.h"

class AAuraBroomVehicle;
class ACharacter;

/**
 * UAuraBroomFollowComponent
 *
 * Owns the broom's autonomous-follow policy: the persistent BT follow thrust, the
 * follow decision (mount hand-off, post-dismount back-off, approach, coast, fixed
 * hover height), and all follow tunables. Extracted from AAuraBroomVehicle so the
 * rideable-pawn concern stays separate from the AI-follow-policy concern.
 *
 * Not replicated: BtFlightThrust is a server-side driver; the movement component
 * re-injects it every server tick and the broom's replicated movement carries the
 * result to clients. The broom exposes thin forwarders (ComputeBtFollowThrust /
 * SetBtFlightThrust / GetBtFlightThrust) so the BT agent and the movement component
 * are unchanged.
 *
 * Reads mount state via the broom's public getters (GetMountedCharacter /
 * GetLastDismountedCharacter / GetTimeSinceLastDismount) rather than owning it, so
 * the follow component depends on the mount component's public read API.
 */
UCLASS(ClassGroup = (Vehicle), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Broom Follow")
class AURA_API UAuraBroomFollowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraBroomFollowComponent();

	/**
	 * The BT follow director. Computes the world-space follow thrust to apply this
	 * tick and, when the broom should also rotate, the yaw to face. Encodes the full
	 * follow decision next to the broom state it reads:
	 *   - mounted:        hands off to the rider (zero thrust);
	 *   - no player:      coast (zero thrust);
	 *   - recent dismount: thrust AWAY from the player until clear, then coast;
	 *   - close enough:    coast (let Deceleration stop the broom);
	 *   - otherwise:       thrust toward the player (at a fixed hover height).
	 *
	 * Side-effect-free (const). Returns FVector::ZeroVector to coast;
	 * bOutHasTargetYaw=false leaves yaw as-is.
	 */
	FVector ComputeFollowThrust(const FVector& PlayerLocation, float& OutTargetYaw, bool& bOutHasTargetYaw) const;

	/** Set the persistent world-space follow thrust (re-injected by the movement component each tick). Zero = coast. */
	void SetFlightThrust(const FVector& WorldThrust) { BtFlightThrust = WorldThrust; }

	FVector GetFlightThrust() const { return BtFlightThrust; }

	// --- Follow tunables ---

	/** If true, the broom only follows when no character is currently mounted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow")
	bool bStopFollowWhenMounted = true;

	/** How fast the broom moves toward/away from the player (flight input scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow", meta = (ClampMin = "0.0"))
	float FollowSpeedScale = 1.0f;

	/**
	 * Distance (cm) at which the broom stops thrusting toward the player and coasts
	 * to a halt via the movement component's Deceleration. Must be >= the braking
	 * distance from MaxSpeed (v^2 / (2*Deceleration)); with MaxSpeed=1200 and
	 * Deceleration=3200 that is ~225cm, so 300 gives margin. Too small and the
	 * broom overshoots/oscillates around the player.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow", meta = (ClampMin = "0.0"))
	float FollowStopRadius = 300.f;

	/**
	 * After a rider dismounts, the broom thrusts AWAY from the player for this many
	 * seconds so the player doesn't immediately walk back into the broom mesh and
	 * remount by accident. Should be >= the mount component's RemountGracePeriodSeconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow", meta = (ClampMin = "0.0"))
	float DismountBackOffDuration = 1.5f;

	/**
	 * Min distance (cm) the broom puts between itself and the player right after a
	 * dismount. While the back-off window is active the broom thrusts away until it
	 * is at least this far from the player, then coasts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow", meta = (ClampMin = "0.0"))
	float DismountBackOffRadius = 800.f;

	/**
	 * If true, the broom holds a fixed hover height while following instead of
	 * diving/climbing to match the player's Z. The follow target is projected onto
	 * the plane at (player.Z + FollowHoverOffset), so the broom follows the player
	 * horizontally and seeks this altitude rather than the player's exact Z.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow")
	bool bUseFixedFollowHeight = true;

	/**
	 * Height (cm) above the player's location the broom seeks while following, when
	 * bUseFixedFollowHeight is true. Keep this small enough that the broom mesh stays
	 * within collision reach of the player for walk-up mounting — too large and the
	 * player can't touch the broom to mount it by collision (OnBroomMeshHit).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Broom|Follow", meta = (ClampMin = "0.0"))
	float FollowHoverOffset = 50.f;

private:
	AAuraBroomVehicle* GetBroomOwner() const;

	// Persistent world-space thrust (direction * scale) commanded by the follow
	// policy. Re-injected into the movement component every server tick so the broom
	// keeps flying between BT pulses. Zero = stop/coast. Not replicated (server-side
	// driver; replicated movement carries the result to clients).
	FVector BtFlightThrust = FVector::ZeroVector;
};