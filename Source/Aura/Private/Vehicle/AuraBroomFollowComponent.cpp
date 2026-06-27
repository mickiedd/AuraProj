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
			// Back away from the player HORIZONTALLY. The old 3D `-ToTarget/Distance`
			// had a downward Z component when the broom was below the hover target,
			// which dove the broom under a rider who had just jumped off. Flattening
			// the away direction to the XY plane keeps the broom level as it retreats;
			// vertical altitude is reclaimed by the post-back-off approach, which
			// seeks the hover height above the player. If the broom is somehow below
			// the player floor, add an upward component so it rises back out instead
			// of skimming under them while backing away.
			FVector AwayDir = (Distance > 1.f)
				? (-ToTarget)
				: Broom->GetActorForwardVector();
			AwayDir.Z = 0.f;
			if (AwayDir.IsNearlyZero(1e-4f))
			{
				AwayDir = Broom->GetActorForwardVector();
				AwayDir.Z = 0.f;
			}
			AwayDir = AwayDir.GetSafeNormal();

			const float FloorZ = PlayerLocation.Z + NeverDescendBelowPlayerOffset;
			FVector ThrustVec = AwayDir;
			if (BroomLocation.Z < FloorZ)
			{
				ThrustVec.Z = FMath::Clamp((FloorZ - BroomLocation.Z) / FMath::Max(FollowHoverOffset, 1.f), 0.f, 1.f);
			}
			ThrustVec = ThrustVec.GetSafeNormal();

			OutTargetYaw = FRotationMatrix::MakeFromX(AwayDir).Rotator().Yaw;
			bOutHasTargetYaw = true;

			// Throttled: ComputeFollowThrust is called every movement tick now, so an
			// unthrottled log here would spam during the ~1.5s back-off window.
			const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
			if (CurrentTime - LastFollowLogTime >= FollowLogInterval)
			{
				UE_LOG(LogAura, Log, TEXT("[BroomFollow] BackOff after dismount: t=%.2fs dist=%.0f awayDir=%s broomZ=%.0f floorZ=%.0f broomLoc=%s"),
					TimeSinceDismount, Distance, *AwayDir.ToCompactString(), BroomLocation.Z, FloorZ, *BroomLocation.ToCompactString());
				LastFollowLogTime = CurrentTime;
			}

			return ThrustVec * FollowSpeedScale;
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
	// component re-applies this every tick (refreshed from the player's live
	// location) so the broom keeps accelerating smoothly toward the player. The
	// thrust's Z component seeks the fixed hover height; the XY components close the
	// horizontal gap to the player.
	const FVector NormalizedDir = ToTarget / Distance;
	OutTargetYaw = FRotationMatrix::MakeFromX(NormalizedDir).Rotator().Yaw;
	bOutHasTargetYaw = true;

	// Ease the thrust off as the broom approaches the stop radius so it decelerates
	// smoothly instead of slamming full-thrust-then-hard-brake (bang-bang). Between
	// FollowStopRadius and FollowStopRadius+FollowEaseRange the scale ramps 0→1;
	// beyond that, full scale. This lets the movement component's Acceleration
	// follow a decreasing target speed and settle gently at the follow distance.
	const float Ease = (FollowEaseRange > KINDA_SMALL_NUMBER)
		? FMath::Clamp((Distance - FollowStopRadius) / FollowEaseRange, 0.f, 1.f)
		: 1.f;
	const float ThrustScale = FollowSpeedScale * Ease;

	// (No per-tick log here — ComputeFollowThrust is called every movement tick now,
	// so logging here would spam. The smoothness diagnostic lives in the broom's
	// RefreshAutonomousFollowThrust, which has the live player location + thrust.)

	FVector ThrustVec = NormalizedDir * ThrustScale;

	// Z floor: the broom must never thrust downward below the player. The hover
	// target is already above the player so this is normally a no-op, but it
	// guarantees the broom can't be driven under the player (e.g. when
	// bUseFixedFollowHeight is off, or via any residual downward seek). The movement
	// component also clamps the position to this floor as a hard guarantee.
	const float FloorZ = PlayerLocation.Z + NeverDescendBelowPlayerOffset;
	if (BroomLocation.Z <= FloorZ && ThrustVec.Z < 0.f)
	{
		ThrustVec.Z = 0.f;
	}

	return ThrustVec;
}