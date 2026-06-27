// Copyright Druid Mechanics

#include "Vehicle/AuraBroomMovement.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "Aura/AuraLogChannels.h"

// UFloatingPawnMovement::TickComponent gates all movement on
// Controller->IsLocalController(). The broom has no possessing controller, so
// we override TickComponent to apply server-authoritative movement directly.

void UAuraBroomMovement::ApplyControlInputToVelocity(float DeltaTime)
{
	// Recompute the autonomous-follow thrust from the player's LIVE location every
	// tick, then feed it as standard movement input so UFloatingPawnMovement
	// accelerates toward it continuously. The Behaviac subsystem only ticks the
	// broom BT at ~10 Hz; if we re-injected the BT's frozen thrust vector the
	// steering direction would lag the player by up to ~100ms and the broom would
	// weave/trail. RefreshAutonomousFollowThrust reads the player's current
	// position directly (60+ Hz) for 1:1 steering and keeps the yaw target live.
	// When mounted the follow policy returns zero thrust, so the rider's own
	// AddFlightInput drives movement unchanged.
	if (AAuraBroomVehicle* Broom = Cast<AAuraBroomVehicle>(PawnOwner))
	{
		Broom->RefreshAutonomousFollowThrust();

		const FVector BtThrust = Broom->GetBtFlightThrust();
		if (!BtThrust.IsNearlyZero(1e-4f))
		{
			AddInputVector(BtThrust, false);
		}
	}

	Super::ApplyControlInputToVelocity(DeltaTime);
}

void UAuraBroomMovement::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (ShouldSkipUpdate(DeltaTime))
	{
		UE_LOG(LogAura, Verbose, TEXT("[BroomMovement] TickComponent skipped (ShouldSkipUpdate). Broom=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	// Call UPawnMovementComponent (grandparent) to run bookkeeping without the
	// controller-gated movement block that lives in UFloatingPawnMovement.
	UPawnMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!PawnOwner || !UpdatedComponent)
	{
		UE_LOG(LogAura, Verbose, TEXT("[BroomMovement] TickComponent aborted -- PawnOwner=%s UpdatedComponent=%s"),
			*GetNameSafe(PawnOwner), *GetNameSafe(UpdatedComponent));
		return;
	}

	// Only the server drives the broom's position; movement is replicated to clients.
	if (!PawnOwner->HasAuthority())
	{
		return;
	}

	const FVector PendingInput = GetPendingInputVector();

	ApplyControlInputToVelocity(DeltaTime);
	LimitWorldBounds();
	bPositionCorrected = false;

	const FVector Delta = Velocity * DeltaTime;

	// UE_LOG(LogAura, Warning, TEXT("[BroomMovement] After ApplyControlInput. NewVelocity=%s Delta=%s"), *Velocity.ToCompactString(), *Delta.ToCompactString());

	if (!Delta.IsNearlyZero(1e-6f))
	{
		const FVector OldLocation = UpdatedComponent->GetComponentLocation();
		const FQuat Rotation = UpdatedComponent->GetComponentQuat();

		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);

		if (Hit.IsValidBlockingHit())
		{
			HandleImpact(Hit, DeltaTime, Delta);
			SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
		}

		if (!bPositionCorrected)
		{
			const FVector NewLocation = UpdatedComponent->GetComponentLocation();
			Velocity = (NewLocation - OldLocation) / DeltaTime;
		}

		// Hard Z floor: the broom must never end up below the player. After a rider
		// jumps off the broom may back away or ascend but it can never dive under the
		// player. The follow policy already avoids producing downward-below-player
		// thrust; this clamp is the hard guarantee that catches any residual downward
		// momentum. Skipped while a rider is mounted (GetMinFlightZ returns false →
		// the rider may dive freely). Runs server-side only (this tick is authority-gated).
		if (AAuraBroomVehicle* Broom = Cast<AAuraBroomVehicle>(PawnOwner))
		{
			float FloorZ = 0.f;
			if (Broom->GetMinFlightZ(FloorZ))
			{
				const FVector PostLoc = UpdatedComponent->GetComponentLocation();
				if (PostLoc.Z < FloorZ)
				{
					UpdatedComponent->SetWorldLocation(FVector(PostLoc.X, PostLoc.Y, FloorZ), false, nullptr, ETeleportType::None);
					if (Velocity.Z < 0.f)
					{
						Velocity.Z = 0.f;
					}
					// Event log (rare — only while the broom is being held at the floor),
					// so the cooked server log proves the hard floor is enforcing
					// "never below the player".
					UE_LOG(LogAura, Log, TEXT("[BroomMovement] Z-floor clamped: broomZ=%.1f floorZ=%.1f (raised + zeroed downward vel)"),
						PostLoc.Z, FloorZ);
				}
			}
		}

		UE_LOG(LogAura, Verbose, TEXT("[BroomMovement] Moved. OldLoc=%s NewLoc=%s BlockingHit=%s"),
			*OldLocation.ToCompactString(),
			*UpdatedComponent->GetComponentLocation().ToCompactString(),
			Hit.IsValidBlockingHit() ? TEXT("YES") : TEXT("no"));
	}

	UpdateComponentVelocity();

	// Log-level diagnostic (throttled) so the server cooked log shows the broom
	// actually integrating flight input into velocity and position each tick.
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (CurrentTime - LastMoveLogTime >= 0.25f)
	{
		const FVector Loc = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
		UE_LOG(LogAura, Log, TEXT("[BroomMovement] server tick. Broom=%s PendingInput=%s Velocity=%s Loc=%s"),
			*GetNameSafe(GetOwner()),
			*PendingInput.ToCompactString(),
			*Velocity.ToCompactString(),
			*Loc.ToCompactString());
		LastMoveLogTime = CurrentTime;
	}
}