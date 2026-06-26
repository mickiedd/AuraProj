// Copyright Druid Mechanics

#include "Vehicle/AuraBroomMovement.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "Aura/AuraLogChannels.h"

// UFloatingPawnMovement::TickComponent gates all movement on
// Controller->IsLocalController(). The broom has no possessing controller, so
// we override TickComponent to apply server-authoritative movement directly.

void UAuraBroomMovement::ApplyControlInputToVelocity(float DeltaTime)
{
	// Re-inject the BT follow thrust as standard movement input every tick so
	// UFloatingPawnMovement accelerates toward it continuously. The BT only fires
	// AddFlightInput ~5-10 Hz and UFloatingPawnMovement consumes input each tick,
	// so without this re-injection the high Deceleration kills velocity between
	// pulses and the broom crawls (~10 cm/s) instead of flying toward the player.
	// When mounted, BtFlightThrust is zero (cleared on mount), so the rider's own
	// AddFlightInput drives movement unchanged.
	if (AAuraBroomVehicle* Broom = Cast<AAuraBroomVehicle>(PawnOwner))
	{
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