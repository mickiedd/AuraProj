// Copyright Druid Mechanics

#include "Vehicle/AuraBroomFlightDriverComponent.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "Vehicle/AuraBroomFollowComponent.h"
#include "Vehicle/AuraBroomMovement.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

UAuraBroomFlightDriverComponent::UAuraBroomFlightDriverComponent()
{
	// Ticks before the movement component (created earlier in the broom constructor)
	// so its AddInputVector is consumed in the same movement tick.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
}

void UAuraBroomFlightDriverComponent::BeginPlay()
{
	Super::BeginPlay();

	// Movement is server-authoritative; there is nothing to drive on clients.
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}
}

AAuraBroomVehicle* UAuraBroomFlightDriverComponent::GetBroomOwner() const
{
	return Cast<AAuraBroomVehicle>(GetOwner());
}

void UAuraBroomFlightDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AAuraBroomVehicle* Broom = GetBroomOwner();
	if (!Broom || !Broom->HasAuthority())
	{
		return;
	}

	UAuraBroomFollowComponent* FollowComponent = Broom->GetFollowComponent();
	UAuraBroomMovement* FlightMovement = Broom->GetFlightMovement();
	if (!FollowComponent || !FlightMovement)
	{
		return;
	}

	// Resolve the player's LIVE location every tick (the BT only refreshes its
	// blackboard ~10 Hz; reading the player directly here gives 60+ Hz steering).
	// Same lookup Method_FindPlayer uses; GetPlayerCharacter is cheap (controller 0
	// pawn). No player → zero thrust → the follow policy coasts to a halt. Cache the
	// result so the movement component's Z-floor clamp (GetMinFlightZ) can use it
	// this tick without a second lookup.
	FVector PlayerLocation = FVector::ZeroVector;
	bool bHasPlayer = false;
	if (UWorld* World = GetWorld())
	{
		if (ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			PlayerLocation = PlayerChar->GetActorLocation();
			bHasPlayer = true;
		}
	}
	LastKnownPlayerLocation = PlayerLocation;
	bHasValidPlayerTarget = bHasPlayer;

	// Run the follow policy (mount hand-off / back-off / approach / coast) with the
	// live player location, then apply the resulting thrust + yaw target.
	float TargetYaw = 0.f;
	bool bHasTargetYaw = false;
	const FVector Thrust = FollowComponent->ComputeFollowThrust(PlayerLocation, TargetYaw, bHasTargetYaw);

	FollowComponent->SetFlightThrust(Thrust);
	if (bHasTargetYaw)
	{
		Broom->SetFlightTargetYaw(TargetYaw);
	}

	// Feed the thrust as standard movement input so UFloatingPawnMovement accelerates
	// toward it continuously. When mounted the policy returns zero thrust, so nothing
	// is added and the rider's own AddFlightInput drives unchanged.
	if (!Thrust.IsNearlyZero(1e-4f))
	{
		FlightMovement->AddInputVector(Thrust, false);
	}

	// Throttled smoothness diagnostic. Read this line to verify the follow is smooth
	// from the cooked server log: `dist` should close monotonically without bouncing,
	// `ease` should ramp 0->1 (no full-on/full-off slamming), `vel` should ramp
	// without spikes, and `dz` (broom.Z - player.Z) must stay >= 0 (never below the
	// player). Reuses the broom's movement-debug log interval as the period.
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (CurrentTime - LastFollowDiagLogTime >= Broom->GetMovementDebugLogInterval())
	{
		const FVector BroomLoc = Broom->GetActorLocation();
		const float Distance = static_cast<float>(FVector::Dist(BroomLoc, PlayerLocation));
		const float ThrustMag = Thrust.Size();
		const float VelMag = Broom->GetVelocity().Size();
		const float FollowSpeedScale = FollowComponent->FollowSpeedScale;
		const float Ease = (FollowSpeedScale > KINDA_SMALL_NUMBER)
			? FMath::Clamp(ThrustMag / FollowSpeedScale, 0.f, 1.f)
			: 0.f;
		const float Dz = BroomLoc.Z - PlayerLocation.Z;

		UE_LOG(LogAura, Log, TEXT("[BroomFollow] diag: dist=%.0f ease=%.3f thrustMag=%.1f vel=%.1f dz=%.0f yaw=%s loc=%s player=%s"),
			Distance,
			Ease,
			ThrustMag,
			VelMag,
			Dz,
			bHasTargetYaw ? *FString::SanitizeFloat(TargetYaw) : TEXT("-"),
			*BroomLoc.ToCompactString(),
			*PlayerLocation.ToCompactString());

		LastFollowDiagLogTime = CurrentTime;
	}
}

bool UAuraBroomFlightDriverComponent::GetMinFlightZ(float& OutFloorZ) const
{
	// No floor while a rider is mounted (the rider may dive freely) or when no player
	// location is known. Otherwise the broom never descends below
	// player.Z + NeverDescendBelowPlayerOffset.
	const AAuraBroomVehicle* Broom = GetBroomOwner();
	if (!Broom || IsValid(Broom->GetMountedCharacter()) || !bHasValidPlayerTarget)
	{
		return false;
	}

	const UAuraBroomFollowComponent* FollowComponent = Broom->GetFollowComponent();
	if (!FollowComponent)
	{
		return false;
	}

	OutFloorZ = LastKnownPlayerLocation.Z + FollowComponent->NeverDescendBelowPlayerOffset;
	return true;
}