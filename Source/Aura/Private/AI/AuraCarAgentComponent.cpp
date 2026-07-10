// Copyright Druid Mechanics

#include "AI/AuraCarAgentComponent.h"
#include "Vehicle/AuraCar.h"
#include "Aura/AuraLogChannels.h"
#include "BehaviorUTypes.h"

UAuraCarAgentComponent::UAuraCarAgentComponent()
{
	// Auto-load the car drive tree shipped in Content/BehaviorTrees/.
	AutoLoadXMLFilePath = TEXT("/Game/BehaviorTrees/BT_AuraCarDrive.xml");
	bAutoTick = true;
}

void UAuraCarAgentComponent::BeginPlay()
{
	// The car BT is server-authoritative: physics movement replicates from the
	// server to clients, so the tree only needs to run on the server.
	const bool bIsServer = GetOwner() && GetOwner()->HasAuthority();

	if (!bIsServer)
	{
		// Prevent Super::BeginPlay from auto-loading the tree on clients.
		AutoLoadXMLFilePath.Empty();
		bAutoTick = false;
	}

	// Super::BeginPlay registers with the BehaviorU world subsystem and, on the
	// server, auto-loads the car drive tree because AutoLoadXMLFilePath is set.
	Super::BeginPlay();

	if (!bIsServer)
	{
		return;
	}

	// Seed the blackboard keys that BT_AuraCarDrive.xml reads and writes.
	SetBoolProperty(TEXT("bHasWaypoint"), false);
	SetVectorProperty(TEXT("WaypointLocation"), FVector::ZeroVector);

	// Bind the C++ behavior methods referenced by the tree's <Action> nodes.
	RegisterMethodHandler(TEXT("PickWaypoint"), [this]() { return Method_PickWaypoint(); });
	RegisterMethodHandler(TEXT("Drive"),        [this]() { return Method_Drive();        });

	UE_LOG(LogAura, Log, TEXT("[CarBehaviorU] Agent bound to %s (server), auto-loading %s"),
		*GetNameSafe(GetOwner()), *AutoLoadXMLFilePath);
}

AAuraCar* UAuraCarAgentComponent::GetCarOwner() const
{
	return Cast<AAuraCar>(GetOwner());
}

EBehaviorUStatus UAuraCarAgentComponent::Method_PickWaypoint()
{
	AAuraCar* Car = GetCarOwner();
	if (!Car)
	{
		return EBehaviorUStatus::Failure;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return EBehaviorUStatus::Failure;
	}

	// Pick a random point around the car at a variable distance.
	const FVector CarLocation = Car->GetActorLocation();

	// Random angle and distance within the configured radius.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Distance = FMath::FRandRange(800.f, 2500.f);

	// Project onto the ground plane (keep Z at the car's current height so the
	// drive component steers horizontally). The drive component handles arrival
	// detection and re-picks when the waypoint is reached.
	const FVector Waypoint(
		CarLocation.X + Distance * FMath::Cos(Angle),
		CarLocation.Y + Distance * FMath::Sin(Angle),
		CarLocation.Z
	);

	SetVectorProperty(TEXT("WaypointLocation"), Waypoint);
	SetBoolProperty(TEXT("bHasWaypoint"), true);

	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAuraCarAgentComponent::Method_Drive()
{
	// Per-tick steering is owned by UAuraCarDriveComponent, which reads
	// Self.WaypointLocation every server tick (60+ Hz) and applies throttle +
	// steering toward it. The BehaviorU subsystem only ticks this tree at ~10 Hz,
	// so writing the thrust here would reintroduce a ~100ms-stale steering vector
	// once per BT tick and make the driving laggy. This action is intentionally a
	// no-op success — the BT's job is waypoint selection + in-scene gating.
	return EBehaviorUStatus::Success;
}