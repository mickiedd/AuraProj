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
#include "Kismet/GameplayStatics.h"

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

	FlightMovement = CreateDefaultSubobject<UAuraBroomMovement>(TEXT("FlightMovement"));
	FlightMovement->UpdatedComponent = Root;
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
	// turns to face wherever the player steers (forward, strafe, or backward).
	if (HasAuthority() && !WorldDirection.IsNearlyZero())
	{
		SetFlightTargetYaw(FRotationMatrix::MakeFromX(WorldDirection).Rotator().Yaw);
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

void AAuraBroomVehicle::RefreshAutonomousFollowThrust()
{
	if (!FollowComponent)
	{
		return;
	}

	// Resolve the player's LIVE location every call (the BT only refreshes its
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

	float TargetYaw = 0.f;
	bool bHasTargetYaw = false;
	const FVector Thrust = FollowComponent->ComputeFollowThrust(PlayerLocation, TargetYaw, bHasTargetYaw);

	FollowComponent->SetFlightThrust(Thrust);
	if (bHasTargetYaw)
	{
		SetFlightTargetYaw(TargetYaw);
	}

	// Throttled smoothness diagnostic. Read this line to verify the follow is smooth
	// from the cooked server log: `dist` should close monotonically without bouncing,
	// `ease` should ramp 0->1 (no full-on/full-off slamming), `vel` should ramp
	// without spikes, and `loc` should track `player` with a small stable gap.
	// (RefreshAutonomousFollowThrust runs every movement tick, so this MUST be
	// throttled — FlightInputLogInterval, default 0.25s.)
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (CurrentTime - LastFollowDiagLogTime >= FlightInputLogInterval)
	{
		const FVector BroomLoc = GetActorLocation();
		const float Distance = static_cast<float>(FVector::Dist(BroomLoc, PlayerLocation));
		const float ThrustMag = Thrust.Size();
		const float VelMag = GetVelocity().Size();
		const float FollowSpeedScale = FollowComponent->FollowSpeedScale;
		const float Ease = (FollowSpeedScale > KINDA_SMALL_NUMBER)
			? FMath::Clamp(ThrustMag / FollowSpeedScale, 0.f, 1.f)
			: 0.f;
		// Broom's altitude relative to the player (broom.Z - player.Z). Must stay
		// >= NeverDescendBelowPlayerOffset (default 0) — a negative value here means
		// the broom is below the player and the Z-floor clamp should be holding it up.
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

bool AAuraBroomVehicle::GetMinFlightZ(float& OutFloorZ) const
{
	// No floor while a rider is mounted (the rider may dive freely) or when no
	// player location is known. Otherwise the broom never descends below
	// player.Z + NeverDescendBelowPlayerOffset.
	if (IsValid(GetMountedCharacter()) || !bHasValidPlayerTarget || !FollowComponent)
	{
		return false;
	}

	OutFloorZ = LastKnownPlayerLocation.Z + FollowComponent->NeverDescendBelowPlayerOffset;
	return true;
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
