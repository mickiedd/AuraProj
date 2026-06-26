// Copyright Druid Mechanics

#include "Vehicle/AuraBroomFollowComponent.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "GameFramework/Character.h" // full ACharacter type for IsValid on mount getters
#include "Aura/AuraLogChannels.h"

UAuraBroomFollowComponent::UAuraBroomFollowComponent()
{
	bAutoActivate = true;
}

AAuraBroomVehicle* UAuraBroomFollowComponent::GetBroomOwner() const
{
	return Cast<AAuraBroomVehicle>(GetOwner());
}

FVector UAuraBroomFollowComponent::ComputeFollowThrust(const FVector& PlayerLocation, float& OutTargetYaw, bool& bOutHasTargetYaw) const
{
	OutTargetYaw = 0.f;
	bOutHasTargetYaw = false;

	AAuraBroomVehicle* Broom = GetBroomOwner();
	if (!Broom)
	{
		return FVector::ZeroVector;
	}

	// Mounted: a rider is controlling the broom manually. Stay out of the way —
	// zero thrust lets the rider's own AddFlightInput be the only thing moving it.
	if (bStopFollowWhenMounted && IsValid(Broom->GetMountedCharacter()))
	{
		return FVector::ZeroVector;
	}

	// No player to follow — coast to a halt.
	if (PlayerLocation.IsNearlyZero(1.f))
	{
		return FVector::ZeroVector;
	}

	const FVector BroomLocation = Broom->GetActorLocation();

	// Hold a fixed hover height while following: project the follow target onto the
	// plane at (player.Z + FollowHoverOffset) so the broom follows the player
	// horizontally and seeks this altitude, instead of diving/climbing to match the
	// player's exact Z. Back-off, stop, and approach all use this projected target.
	const FVector FollowTarget = bUseFixedFollowHeight
		? FVector(PlayerLocation.X, PlayerLocation.Y, PlayerLocation.Z + FollowHoverOffset)
		: PlayerLocation;

	const FVector ToTarget = FollowTarget - BroomLocation;
	const float Distance = ToTarget.Size();

	// --- Post-dismount back-off ---
	// Right after a rider dismounts, thrust AWAY from the player until clear so
	// they don't immediately walk back into the broom mesh and remount by accident.
	// Only reacts to the character that just dismounted.
	const float TimeSinceDismount = Broom->GetTimeSinceLastDismount();
	const bool bRecentDismount = IsValid(Broom->GetLastDismountedCharacter())
		&& TimeSinceDismount >= 0.f
		&& TimeSinceDismount < DismountBackOffDuration;

	if (bRecentDismount)
	{
		if (Distance < DismountBackOffRadius)
		{
			// Direction from player to broom = away from the player. If the broom is
			// exactly on top of the player (Distance ~ 0), escape along its forward
			// vector so we still pick a definite heading.
			const FVector AwayDir = (Distance > 1.f)
				? (-ToTarget / Distance)
				: Broom->GetActorForwardVector().GetSafeNormal();

			OutTargetYaw = FRotationMatrix::MakeFromX(AwayDir).Rotator().Yaw;
			bOutHasTargetYaw = true;

			UE_LOG(LogAura, Log, TEXT("[BroomFollow] BackOff after dismount: t=%.2fs dist=%.0f awayDir=%s broomLoc=%s"),
				TimeSinceDismount, Distance, *AwayDir.ToCompactString(), *BroomLocation.ToCompactString());

			return AwayDir * FollowSpeedScale;
		}

		// Cleared the player — coast at this distance until the back-off window
		// expires, then resume normal follow.
		return FVector::ZeroVector;
	}

	// Close enough — coast to a halt instead of jittering on top of the player.
	// FollowStopRadius must exceed the braking distance from MaxSpeed
	// (v^2 / (2*Deceleration)) or the broom will overshoot.
	if (Distance <= FollowStopRadius)
	{
		return FVector::ZeroVector;
	}

	// Approach: persistent thrust toward the (height-projected) target. The movement
	// component re-applies this every tick so the broom keeps accelerating between
	// the BT's ~5-10 Hz pulses — otherwise the high Deceleration kills velocity
	// between pulses and the broom only crawls. The thrust's Z component seeks the
	// fixed hover height; the XY components close the horizontal gap to the player.
	const FVector NormalizedDir = ToTarget / Distance;
	OutTargetYaw = FRotationMatrix::MakeFromX(NormalizedDir).Rotator().Yaw;
	bOutHasTargetYaw = true;

	UE_LOG(LogAura, Log, TEXT("[BroomFollow] Approach: dist=%.0f dir=%s broomLoc=%s broomVel=%s"),
		Distance,
		*NormalizedDir.ToCompactString(),
		*BroomLocation.ToCompactString(),
		*Broom->GetVelocity().ToCompactString());

	return NormalizedDir * FollowSpeedScale;
}