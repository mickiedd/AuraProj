// Copyright Druid Mechanics


#include "Vehicle/AuraBroomVehicle.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/AuraCharacterBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Net/UnrealNetwork.h"
#include "Player/AuraPlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "AI/AuraBroomAgentComponent.h"
#include "Vehicle/AuraBroomFlightDriverComponent.h"

// ─── AAuraBroomVehicle ────────────────────────────────────────────────────────

AAuraBroomVehicle::AAuraBroomVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(100.f);
	SetMinNetUpdateFrequency(30.f);
	NetPriority = 3.f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BroomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BroomMesh"));
	BroomMesh->SetupAttachment(Root);
	BroomMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BroomMesh->SetNotifyRigidBodyCollision(true);
	BroomMesh->SetIsReplicated(true);
	// OnComponentHit is bound by the mount component in its BeginPlay.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BroomMeshAsset(TEXT("/Game/Assets/Vehicle/Mesh/Broom.Broom"));
	if (BroomMeshAsset.Succeeded())
	{
		BroomMesh->SetStaticMesh(BroomMeshAsset.Object);
	}

	// Autonomous-flight driver. Created BEFORE FlightMovement so it ticks first and
	// its AddInputVector is consumed by the movement component in the same tick.
	FlightDriverComponent = CreateDefaultSubobject<UAuraBroomFlightDriverComponent>(TEXT("FlightDriverComponent"));

	FlightMovement = CreateDefaultSubobject<UAuraBroomMovement>(TEXT("FlightMovement"));
	// Sweep the BroomMesh (a UPrimitiveComponent with collision), not Root. Root is a
	// plain USceneComponent, and USceneComponent::MoveComponent ignores bSweep and just
	// teleports — so sweeping Root let the broom pass through every building. Sweeping the
	// mesh makes SafeMoveUpdatedComponent / SlideAlongSurface use the mesh's real collision
	// shape. The mesh is a child of Root, so sweeping it moves only the mesh; the movement
	// component re-applies the mesh's net delta to the actor root each tick (see
	// UAuraBroomMovement::TickComponent) so the replicated transform stays authoritative.
	// We deliberately do NOT reparent the mesh to be the root: BP_Broom has its own cooked
	// component tree, and reparenting against it cycles the attachment hierarchy and
	// stack-overflows the server during broom spawn.
	FlightMovement->UpdatedComponent = BroomMesh;
	FlightMovement->MaxSpeed = 1200.f;
	FlightMovement->Acceleration = 2400.f;
	FlightMovement->Deceleration = 3200.f;

	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	// BehaviorU agent: auto-loads BT_BroomFollowPlayer.xml on BeginPlay and
	// drives the broom to follow the nearest player character.
	BroomAgentComponent = CreateDefaultSubobject<UAuraBroomAgentComponent>(TEXT("BroomAgentComponent"));

	// Idle-hover + flight-yaw smoothing. Driven from this pawn's Tick via TickMotion.
	MotionComponent = CreateDefaultSubobject<UAuraBroomMotionComponent>(TEXT("MotionComponent"));

	// Mount/dismount state machine + collision-driven mounting. Replicated; owns
	// the MountedCharacter reference and the remount grace window.
	MountComponent = CreateDefaultSubobject<UAuraBroomMountComponent>(TEXT("MountComponent"));

	// Autonomous BT follow policy + the persistent follow thrust it produces.
	FollowComponent = CreateDefaultSubobject<UAuraBroomFollowComponent>(TEXT("FollowComponent"));
}

void AAuraBroomVehicle::BeginPlay()
{
	Super::BeginPlay();
}

void AAuraBroomVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Hover + yaw smoothing live on the motion component; call it here so the
	// ordering relative to the movement component's tick is preserved.
	if (MotionComponent)
	{
		MotionComponent->TickMotion(DeltaSeconds);
	}
}

void AAuraBroomVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AAuraBroomVehicle::AddFlightInput(const FVector& WorldDirection, float ScaleValue)
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bCanLogFlight = CurrentTime - LastFlightInputLogTime >= FlightInputLogInterval;
	const bool bCanLogFlightBlocked = CurrentTime - LastFlightBlockedLogTime >= FlightInputLogInterval;

	// Stamp the last-input time so the motion component's idle hover keeps yielding
	// to flight between BT input pulses (the BT fires AddFlightInput ~10 Hz).
	LastFlightInputAppliedTime = CurrentTime;

	UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] AddFlightInput called. Broom=%s HasAuthority=%s Rider=%s Dir=%s Scale=%.3f FlightMovement=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetMountedCharacter()),
		*WorldDirection.ToCompactString(),
		ScaleValue,
		*GetNameSafe(FlightMovement));

	if (!IsValid(GetMountedCharacter()) && bCanLogFlightBlocked)
	{
		UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] Flight input ignored: no mounted rider. Broom=%s"), *GetNameSafe(this));
		LastFlightBlockedLogTime = CurrentTime;
	}

	if (!IsValid(FlightMovement))
	{
		UE_LOG(LogAura, Error, TEXT("[BroomFlight] ERROR: FlightMovement is null. Broom=%s"), *GetNameSafe(this));
		return;
	}

	AddMovementInput(WorldDirection, ScaleValue);

	// Derive the facing target from the movement direction so the broom smoothly
	// turns to face wherever the player steers (forward, strafe, or backward). Use
	// only the horizontal component: a purely vertical direction (Q/E ascend/descend)
	// has no meaningful yaw, and MakeFromX on an up-pointing vector would collapse to
	// yaw 0 and snap the broom away from the camera-facing yaw.
	const FVector HorizontalDir(WorldDirection.X, WorldDirection.Y, 0.f);
	if (HasAuthority() && !HorizontalDir.IsNearlyZero())
	{
		SetFlightTargetYaw(FRotationMatrix::MakeFromX(HorizontalDir).Rotator().Yaw);
	}

	UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] AddMovementInput sent. PendingInputVector=%s"),
		*GetPendingMovementInputVector().ToCompactString());

	if (bCanLogFlight)
	{
		UE_LOG(LogAura, Log, TEXT("Flight input applied. Broom=%s Rider=%s Direction=%s Scale=%0.2f Velocity=%s Location=%s HasAuthority=%s"),
			*GetNameSafe(this),
			*GetNameSafe(GetMountedCharacter()),
			*WorldDirection.ToCompactString(),
			ScaleValue,
			*GetVelocity().ToCompactString(),
			*GetActorLocation().ToCompactString(),
			HasAuthority() ? TEXT("true") : TEXT("false"));
		LastFlightInputLogTime = CurrentTime;
	}
}

void AAuraBroomVehicle::RequestMount(ACharacter* CharacterToMount)
{
	// Mount logic (authority path + server RPC) lives on the mount component.
	if (MountComponent)
	{
		MountComponent->RequestMount(CharacterToMount);
	}
}

void AAuraBroomVehicle::RequestDismount()
{
	if (MountComponent)
	{
		MountComponent->RequestDismount();
	}
}

void AAuraBroomVehicle::SetFlightTargetYaw(float WorldYaw)
{
	// Forward to the motion component, which owns the yaw target + smoothing.
	if (MotionComponent)
	{
		MotionComponent->SetTargetYaw(WorldYaw);
	}
}

bool AAuraBroomVehicle::GetMinFlightZ(float& OutFloorZ) const
{
	// Forward to the flight driver, which owns the cached player location and the
	// mount/no-player gating for the floor.
	return FlightDriverComponent ? FlightDriverComponent->GetMinFlightZ(OutFloorZ) : false;
}

bool AAuraBroomVehicle::IsFlightInputActive() const
{
	// True while the broom is actively flying: non-zero velocity, pending movement
	// input, or a recent AddFlightInput within the yield window. The motion
	// component's idle hover uses this to yield to flight so it doesn't pin the
	// broom back to its hover anchor and cancel in-flight movement.
	if (IsValid(FlightMovement))
	{
		if (FlightMovement->Velocity.SizeSquared() > 1.f)
		{
			return true;
		}
		if (!FlightMovement->GetPendingInputVector().IsNearlyZero(0.01f))
		{
			return true;
		}
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	return (CurrentTime - LastFlightInputAppliedTime) < 0.25f;
}
