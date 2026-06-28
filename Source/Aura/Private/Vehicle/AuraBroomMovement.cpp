// Copyright Druid Mechanics

#include "Vehicle/AuraBroomMovement.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "Aura/AuraLogChannels.h"

// UFloatingPawnMovement::TickComponent gates all movement on
// Controller->IsLocalController(). The broom has no possessing controller, so
// we override TickComponent to apply server-authoritative movement directly.
//
// This component only integrates movement: the UAuraBroomFlightDriverComponent
// (ticking before this one) and the rider's AddFlightInput feed the pending input
// each tick; UFloatingPawnMovement's default ApplyControlInputToVelocity consumes
// it. We also enforce the altitude floor (never below the player) — a movement
// constraint whose value the broom supplies via GetMinFlightZ.

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

		// Re-sync the actor root to the mesh's net movement.
		//
		// UpdatedComponent is the BroomMesh — a UPrimitiveComponent child of Root — so the
		// SafeMoveUpdatedComponent / SlideAlongSurface calls above actually sweep collision
		// (sweeping a plain USceneComponent only teleports, which is why the broom used to
		// pass through buildings). But sweeping a child moves only the mesh; the actor root
		// (Root, which carries the replicated transform and GetActorLocation()) would be
		// left behind, desyncing replication and the follow logic. So: undo the mesh's world
		// move (teleport it back to its pre-sweep pose) and translate the actor root by the
		// same net delta. The mesh follows via attachment and ends at the same final pose,
		// with the root now carrying the movement. We do NOT reparent the mesh to be the
		// root to achieve this: BP_Broom has its own cooked component tree and reparenting
		// against it cycles the attachment hierarchy (stack overflow during broom spawn).
		const FVector MeshFinal = UpdatedComponent->GetComponentLocation();
		const FVector NetDelta = MeshFinal - OldLocation;
		if (!NetDelta.IsNearlyZero(1e-6f))
		{
			UpdatedComponent->SetWorldLocation(OldLocation, false, nullptr, ETeleportType::TeleportPhysics);
			if (USceneComponent* ActorRoot = PawnOwner->GetRootComponent())
			{
				ActorRoot->AddWorldOffset(NetDelta, false, nullptr, ETeleportType::None);
			}
		}

		UE_LOG(LogAura, Verbose, TEXT("[BroomMovement] Moved. OldLoc=%s NewLoc=%s BlockingHit=%s"),
			*OldLocation.ToCompactString(),
			*UpdatedComponent->GetComponentLocation().ToCompactString(),
			Hit.IsValidBlockingHit() ? TEXT("YES") : TEXT("no"));
	}

	UpdateComponentVelocity();
}