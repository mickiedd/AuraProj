// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "AuraBroomMovement.generated.h"

class AAuraBroomVehicle;

/**
 * Server-authoritative movement component for the broom.
 *
 * Single responsibility: integrate pending control input into velocity and move
 * the UpdatedComponent with collision, server-authoritatively — nothing more. It
 * does NOT know about the follow system; a UAuraBroomFlightDriverComponent (owned
 * by the broom, ticking before this component) feeds flight thrust into the
 * pending input each tick, and the rider's AddFlightInput does the same when
 * mounted. This component just consumes that input via UFloatingPawnMovement's
 * default ApplyControlInputToVelocity.
 *
 * It also enforces an altitude floor (never below the player) — a movement
 * constraint whose *value* is supplied by the broom (GetMinFlightZ, which forwards
 * to the flight driver), not decided here.
 *
 * UFloatingPawnMovement normally gates movement on Controller->IsLocalController(),
 * which prevents movement when the broom has no possessing controller. This
 * subclass skips that check (by calling UPawnMovementComponent::TickComponent
 * directly) and drives movement from server authority instead, with the result
 * replicated to clients.
 */
UCLASS()
class AURA_API UAuraBroomMovement : public UFloatingPawnMovement
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};