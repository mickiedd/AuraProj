// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "AuraBroomMovement.generated.h"

class AAuraBroomVehicle;

/**
 * Server-authoritative movement component for the broom.
 *
 * UFloatingPawnMovement normally gates movement on Controller->IsLocalController(),
 * which prevents movement when the broom has no possessing controller. This
 * subclass skips that check (by calling UPawnMovementComponent::TickComponent
 * directly) and drives movement from server authority instead, with the result
 * replicated to clients.
 *
 * Lives in its own files, separate from AAuraBroomVehicle, so the movement-
 * integration concern stays distinct from the rideable-pawn concern. The pawn
 * owns an instance of this component; this component reads the broom's BT follow
 * thrust (GetBtFlightThrust) to keep the broom accelerating between BT pulses.
 */
UCLASS()
class AURA_API UAuraBroomMovement : public UFloatingPawnMovement
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Re-injects the BT follow thrust as standard movement input every tick so the
	// broom continuously accelerates toward the player. The BT only fires
	// AddFlightInput ~5-10 Hz; UFloatingPawnMovement consumes input each tick, so
	// without re-injection the high Deceleration kills velocity between pulses and
	// the broom crawls (~10 cm/s) instead of flying.
	virtual void ApplyControlInputToVelocity(float DeltaTime) override;

private:
	// Throttle for the diagnostic move log (Log-level, so it shows in cooked server logs).
	float LastMoveLogTime = -1000.f;
};