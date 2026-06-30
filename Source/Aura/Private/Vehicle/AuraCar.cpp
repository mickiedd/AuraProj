// Copyright Druid Mechanics

#include "Vehicle/AuraCar.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "AI/AuraCarAgentComponent.h"
#include "Vehicle/AuraCarDriveComponent.h"

AAuraCar::AAuraCar()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	// Root collision box: QueryOnly (no physics). The car is driven kinematically
	// by UAuraCarDriveComponent (SetActorLocationAndRotation every tick), so it
	// does not need a simulated rigid body. Collision queries (overlaps, hits,
	// line traces) still work — the box just won't respond to physics forces.
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetMobility(EComponentMobility::Movable);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionProfileName(TEXT("BlockAll"));
	CollisionBox->SetBoxExtent(FVector(100.f, 50.f, 40.f));
	CollisionBox->SetSimulatePhysics(false);
	CollisionBox->SetEnableGravity(false);

	CarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarMesh"));
	CarMesh->SetupAttachment(CollisionBox);
	CarMesh->SetMobility(EComponentMobility::Movable);
	CarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Wheels: attached to the body at identity transform (the Cartoon City pack
	// authors each wheel mesh with its pivot at the body origin, so this
	// reconstructs the assembled car). Wheels carry no collision; the box is
	// the primary collider.
	WheelFrontLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFrontLeft"));
	WheelFrontLeft->SetupAttachment(CarMesh);
	WheelFrontLeft->SetMobility(EComponentMobility::Movable);
	WheelFrontLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WheelFrontRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelFrontRight"));
	WheelFrontRight->SetupAttachment(CarMesh);
	WheelFrontRight->SetMobility(EComponentMobility::Movable);
	WheelFrontRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WheelRearLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRearLeft"));
	WheelRearLeft->SetupAttachment(CarMesh);
	WheelRearLeft->SetMobility(EComponentMobility::Movable);
	WheelRearLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WheelRearRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WheelRearRight"));
	WheelRearRight->SetupAttachment(CarMesh);
	WheelRearRight->SetMobility(EComponentMobility::Movable);
	WheelRearRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default to the SM_Car_06 body + wheels from the Cartoon City pack. Override
	// the Default* properties on a BP subclass or per-instance to use 13 / 16 / 19.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> BodyMesh(
		TEXT("/Game/Cartoon_City_Free/Meshes/Cars/SM_Car_06"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelFLMesh(
		TEXT("/Game/Cartoon_City_Free/Meshes/Cars/SM_Car_06_Wheel_Front_Left"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelFRMesh(
		TEXT("/Game/Cartoon_City_Free/Meshes/Cars/SM_Car_06_Wheel_Front_Right"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelRLMesh(
		TEXT("/Game/Cartoon_City_Free/Meshes/Cars/SM_Car_06_Wheel_Rear_Left"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> WheelRRMesh(
		TEXT("/Game/Cartoon_City_Free/Meshes/Cars/SM_Car_06_Wheel_Rear_Right"));

	if (BodyMesh.Succeeded())
	{
		DefaultCarMesh = BodyMesh.Object;
		CarMesh->SetStaticMesh(DefaultCarMesh);
	}
	if (WheelFLMesh.Succeeded())
	{
		DefaultWheelFrontLeft = WheelFLMesh.Object;
		WheelFrontLeft->SetStaticMesh(DefaultWheelFrontLeft);
	}
	if (WheelFRMesh.Succeeded())
	{
		DefaultWheelFrontRight = WheelFRMesh.Object;
		WheelFrontRight->SetStaticMesh(DefaultWheelFrontRight);
	}
	if (WheelRLMesh.Succeeded())
	{
		DefaultWheelRearLeft = WheelRLMesh.Object;
		WheelRearLeft->SetStaticMesh(DefaultWheelRearLeft);
	}
	if (WheelRRMesh.Succeeded())
	{
		DefaultWheelRearRight = WheelRRMesh.Object;
		WheelRearRight->SetStaticMesh(DefaultWheelRearRight);
	}

	// Autonomous driving: Behaviac BT agent (waypoint selection at ~10 Hz) +
	// per-tick drive component (steering + throttle at 60+ Hz). Server-authoritative;
	// both skip work on clients (see their BeginPlay guards).
	CarAgentComponent = CreateDefaultSubobject<UAuraCarAgentComponent>(TEXT("CarAgentComponent"));
	CarDriveComponent = CreateDefaultSubobject<UAuraCarDriveComponent>(TEXT("CarDriveComponent"));
}

void AAuraCar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AAuraCar::BeginPlay()
{
	Super::BeginPlay();
}