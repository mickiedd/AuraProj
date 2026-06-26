// Copyright Druid Mechanics

#include "Vehicle/AuraBroomMotionComponent.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "Vehicle/AuraBroomMovement.h"
#include "GameFramework/Character.h" // full ACharacter type for IsValid(GetMountedCharacter())
#include "Aura/AuraLogChannels.h"

UAuraBroomMotionComponent::UAuraBroomMotionComponent()
{
	// Driven by the owning broom's Tick via TickMotion(), not by its own tick, so
	// it runs after the movement component has integrated flight input.
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

AAuraBroomVehicle* UAuraBroomMotionComponent::GetBroomOwner() const
{
	return Cast<AAuraBroomVehicle>(GetOwner());
}

void UAuraBroomMotionComponent::SetTargetYaw(float WorldYaw)
{
	FlightTargetYaw = WorldYaw;
	bHasFlightTargetYaw = true;
}

void UAuraBroomMotionComponent::TickMotion(float DeltaSeconds)
{
	// Same order the broom used before extraction: hover (position + tilt) first,
	// then yaw smoothing. Both write the actor transform; keeping them in one
	// component makes the ordering explicit.
	UpdateIdleHover(DeltaSeconds);
	UpdateFlightYaw(DeltaSeconds);
}

void UAuraBroomMotionComponent::UpdateFlightYaw(float DeltaSeconds)
{
	// Only the server drives broom rotation; movement replication carries it to
	// clients. Applies both when a rider is mounted (player steer input) and when
	// the BT is following a player unmounted (the follow director sets the yaw).
	AAuraBroomVehicle* Broom = GetBroomOwner();
	if (!Broom || !Broom->HasAuthority() || !bHasFlightTargetYaw)
	{
		return;
	}

	const FRotator CurrentRot = Broom->GetActorRotation();
	const FRotator TargetRot(0.f, FlightTargetYaw, 0.f);

	// RInterpTo handles the shortest-path wrap at the ±180° boundary.
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaSeconds, YawInterpSpeed);
	Broom->SetActorRotation(NewRot);
}

void UAuraBroomMotionComponent::UpdateIdleHover(float DeltaSeconds)
{
	AAuraBroomVehicle* Broom = GetBroomOwner();
	if (!Broom || !Broom->HasAuthority() || !bEnableIdleHover)
	{
		return;
	}

	const bool bShouldHover = !IsValid(Broom->GetMountedCharacter());
	if (!bShouldHover)
	{
		bWasHoveringLastTick = false;
		IdleHoverCurrentOffset = FVector::ZeroVector;
		IdleHoverCurrentRotationOffset = FRotator::ZeroRotator;
		IdleHoverBaseLocation = Broom->GetActorLocation();
		IdleHoverBaseRotation = Broom->GetActorRotation();
		return;
	}

	// If the BT (AddFlightInput from the follow director) or any other system is
	// actively flying the broom, the movement component must be authoritative for
	// position. The idle hover pins the actor to a frozen IdleHoverBaseLocation via
	// SetActorLocationAndRotation every frame; if we run that while flight velocity
	// is non-zero, the hover teleports the broom back to its anchor and completely
	// cancels the flight movement — so AddFlightInput appears to do nothing. Yield
	// here and let the movement component drive until the broom coasts to a stop.
	if (Broom->IsFlightInputActive())
	{
		// Reset so the next truly-idle frame re-captures the base around the
		// broom's current (flown) position instead of the stale spawn location.
		bWasHoveringLastTick = false;

		const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (CurrentTime - LastHoverLogTime >= HoverLogInterval)
		{
			UAuraBroomMovement* FlightMovement = Broom->GetFlightMovement();
			UE_LOG(LogAura, Log, TEXT("[BroomHover] yielding to flight (no pin). Broom=%s Vel=%s Pending=%s"),
				*GetNameSafe(Broom),
				FlightMovement ? *FlightMovement->Velocity.ToCompactString() : TEXT("null"),
				FlightMovement ? *FlightMovement->GetPendingInputVector().ToCompactString() : TEXT("null"));
			LastHoverLogTime = CurrentTime;
		}
		return;
	}

	if (!bWasHoveringLastTick)
	{
		IdleHoverBaseLocation = Broom->GetActorLocation();
		IdleHoverBaseRotation = Broom->GetActorRotation();
		IdleHoverCurrentOffset = FVector::ZeroVector;
		IdleHoverCurrentRotationOffset = FRotator::ZeroRotator;
		IdleHoverTimeSeconds = 0.f;
	}

	IdleHoverTimeSeconds += DeltaSeconds;

	const float BobPhase = IdleHoverTimeSeconds * HoverBobFrequency * 2.f * PI;
	const float SwayPhase = IdleHoverTimeSeconds * HoverSwayFrequency * 2.f * PI;
	const float TiltPhase = IdleHoverTimeSeconds * HoverTiltFrequency * 2.f * PI;

	const FVector TargetOffset(
		FMath::Sin(SwayPhase + 1.3f) * HoverSwayAmplitude,
		FMath::Cos((SwayPhase * 0.73f) + 0.6f) * HoverSwayAmplitude * 0.55f,
		FMath::Sin(BobPhase) * HoverBobAmplitude);

	const FRotator TargetRotationOffset(
		FMath::Sin(TiltPhase + 0.4f) * HoverPitchAmplitude,
		0.f,
		FMath::Cos((TiltPhase * 1.17f) + 0.2f) * HoverRollAmplitude);

	IdleHoverCurrentOffset = FMath::VInterpTo(IdleHoverCurrentOffset, TargetOffset, DeltaSeconds, HoverSmoothingSpeed);
	IdleHoverCurrentRotationOffset = FMath::RInterpTo(IdleHoverCurrentRotationOffset, TargetRotationOffset, DeltaSeconds, HoverSmoothingSpeed);

	const FVector NewLocation = IdleHoverBaseLocation + IdleHoverCurrentOffset;
	const FRotator NewRotation = IdleHoverBaseRotation + IdleHoverCurrentRotationOffset;
	Broom->SetActorLocationAndRotation(NewLocation, NewRotation);

	bWasHoveringLastTick = true;
}