// Copyright Druid Mechanics

#include "Vehicle/AuraCarDriveComponent.h"
#include "Vehicle/AuraCar.h"
#include "AI/AuraCarAgentComponent.h"
#include "Aura/AuraLogChannels.h"

UAuraCarDriveComponent::UAuraCarDriveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAuraCarDriveComponent::BeginPlay()
{
	Super::BeginPlay();

	// Server-authoritative: disable ticking on clients so the server's
	// replicated transform is the single source of truth.
	if (bServerAuthoritative && GetOwner() && !GetOwner()->HasAuthority())
	{
		SetComponentTickEnabled(false);
	}
}

void UAuraCarDriveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AAuraCar* Car = GetCarOwner();
	if (!Car)
	{
		return;
	}

	// Read the current waypoint from the Behaviac agent's blackboard.
	UAuraCarAgentComponent* Agent = Car->GetCarAgentComponent();
	if (!Agent)
	{
		return;
	}

	const bool bHasWaypoint = Agent->GetBoolProperty(TEXT("bHasWaypoint"));
	if (!bHasWaypoint)
	{
		return; // No waypoint: car stays still.
	}

	const FVector Waypoint = Agent->GetVectorProperty(TEXT("WaypointLocation"));
	const FVector CarLocation = Car->GetActorLocation();
	const FVector ToWaypoint = Waypoint - CarLocation;
	const float Distance = ToWaypoint.Size2D(); // horizontal distance

	// Arrival check: if close enough, clear the waypoint and stop.
	if (Distance < ArrivalRadius)
	{
		Agent->SetBoolProperty(TEXT("bHasWaypoint"), false);
		return;
	}

	// --- Steering: compute yaw delta between car forward and waypoint direction ---
	const FVector CarForward = Car->GetActorForwardVector();
	const FVector ToWaypointDir = ToWaypoint.GetSafeNormal2D();

	// Yaw angle in degrees [-180, 180] between forward and target direction.
	float YawDeltaDeg = FMath::RadiansToDegrees(
		FMath::Atan2(ToWaypointDir.Y, ToWaypointDir.X) -
		FMath::Atan2(CarForward.Y, CarForward.X)
	);
	YawDeltaDeg = FMath::UnwindDegrees(YawDeltaDeg);
	// Clamp to max steer angle.
	YawDeltaDeg = FMath::Clamp(YawDeltaDeg, -MaxSteerAngle, MaxSteerAngle);

	// Rotate the car toward the waypoint, capped by TurnRate * DeltaTime so the
	// rotation is smooth and doesn't overshoot the target yaw.
	const float MaxYawStep = TurnRate * DeltaTime;
	const float YawStep = FMath::Clamp(YawDeltaDeg, -MaxYawStep, MaxYawStep);
	const FRotator CurrentRotation = Car->GetActorRotation();
	const FRotator NewRotation = CurrentRotation + FRotator(0.f, YawStep, 0.f);

	// --- Move forward along the new forward vector ---
	// Ease speed down as the car approaches the waypoint.
	const float EaseStart = ArrivalRadius + ArrivalEaseRange;
	const float SpeedScale = FMath::Clamp((Distance - ArrivalRadius) / (EaseStart - ArrivalRadius), 0.f, 1.f);
	const float CurrentSpeed = MaxSpeed * SpeedScale;
	const FVector NewForward = NewRotation.Vector();
	const FVector NewLocation = CarLocation + NewForward * CurrentSpeed * DeltaTime;

	// Kinematic move: set location + rotation directly (no physics).
	Car->SetActorLocationAndRotation(NewLocation, NewRotation);

	// Throttled diagnostic log.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Now - LastDriveLogTime >= DriveLogInterval)
	{
		LastDriveLogTime = Now;
		UE_LOG(LogAura, Log, TEXT("[CarDrive] %s wp=%s dist=%.0f yawDelta=%.1f yawStep=%.1f speed=%.0f"),
			*Car->GetName(), *Waypoint.ToCompactString(), Distance, YawDeltaDeg, YawStep, CurrentSpeed);
	}
}

AAuraCar* UAuraCarDriveComponent::GetCarOwner() const
{
	return Cast<AAuraCar>(GetOwner());
}