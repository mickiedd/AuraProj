// Copyright Druid Mechanics


#include "Vehicle/AuraBroomVehicle.h"

#include "Aura/AuraLogChannels.h"
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

// ─── UAuraBroomMovement ───────────────────────────────────────────────────────
// UFloatingPawnMovement::TickComponent gates all movement on
// Controller->IsLocalController(). The broom has no possessing controller, so
// we override TickComponent to apply server-authoritative movement directly.

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

	const FVector PendingInput = GetPendingInputVector();

	/* UE_LOG(LogAura, Warning, TEXT("[BroomMovement] TickComponent (server). Broom=%s PendingInput=%s CurrentVelocity=%s DT=%.4f"),
		*GetNameSafe(GetOwner()),
		*PendingInput.ToCompactString(),
		*Velocity.ToCompactString(),
		DeltaTime); */

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

		UE_LOG(LogAura, Verbose, TEXT("[BroomMovement] Moved. OldLoc=%s NewLoc=%s BlockingHit=%s"),
			*OldLocation.ToCompactString(),
			*UpdatedComponent->GetComponentLocation().ToCompactString(),
			Hit.IsValidBlockingHit() ? TEXT("YES") : TEXT("no"));
	}

	UpdateComponentVelocity();
}

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
	BroomMesh->OnComponentHit.AddDynamic(this, &AAuraBroomVehicle::OnBroomMeshHit);

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
}

void AAuraBroomVehicle::BeginPlay()
{
	Super::BeginPlay();

	IdleHoverBaseLocation = GetActorLocation();
	IdleHoverBaseRotation = GetActorRotation();
}

void AAuraBroomVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateIdleHover(DeltaSeconds);
	UpdateFlightYaw(DeltaSeconds);
}

void AAuraBroomVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAuraBroomVehicle, MountedCharacter);
}

void AAuraBroomVehicle::AddFlightInput(const FVector& WorldDirection, float ScaleValue)
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const bool bCanLogFlight = CurrentTime - LastFlightInputLogTime >= FlightInputLogInterval;
	const bool bCanLogFlightBlocked = CurrentTime - LastFlightBlockedLogTime >= FlightInputLogInterval;

	UE_LOG(LogAura, Verbose, TEXT("[BroomFlight] AddFlightInput called. Broom=%s HasAuthority=%s Rider=%s Dir=%s Scale=%.3f FlightMovement=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(MountedCharacter),
		*WorldDirection.ToCompactString(),
		ScaleValue,
		*GetNameSafe(FlightMovement));

	if (!IsValid(MountedCharacter) && bCanLogFlightBlocked)
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
		UE_LOG(LogAura, Verbose, TEXT("Flight input applied. Broom=%s Rider=%s Direction=%s Scale=%0.2f Velocity=%s Location=%s HasAuthority=%s"),
			*GetNameSafe(this),
			*GetNameSafe(MountedCharacter),
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
	if (!IsValid(CharacterToMount))
	{
		return;
	}

	if (HasAuthority())
	{
		MountCharacterInternal(CharacterToMount);
		return;
	}

	ServerRequestMount(CharacterToMount);
}

void AAuraBroomVehicle::RequestDismount()
{
	UE_LOG(LogAura, Log, TEXT("Broom[%s] RequestDismount received. HasAuthority=%s MountedCharacter=%s"),
		*GetNameSafe(this),
		HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(MountedCharacter));

	if (HasAuthority())
	{
		DismountCharacterInternal();
		return;
	}

	ServerRequestDismount();
}

void AAuraBroomVehicle::ServerRequestMount_Implementation(ACharacter* CharacterToMount)
{
	MountCharacterInternal(CharacterToMount);
}

void AAuraBroomVehicle::ServerRequestDismount_Implementation()
{
	UE_LOG(LogAura, Log, TEXT("Broom[%s] ServerRequestDismount_Implementation. MountedCharacter=%s"),
		*GetNameSafe(this),
		*GetNameSafe(MountedCharacter));

	DismountCharacterInternal();
}

void AAuraBroomVehicle::OnRep_MountedCharacter()
{
	if (LastMountedCharacter != MountedCharacter)
	{
		if (IsValid(LastMountedCharacter))
		{
			ApplyMountedState(LastMountedCharacter, false);
		}

		if (IsValid(MountedCharacter))
		{
			ApplyMountedState(MountedCharacter, true);
		}

		LastMountedCharacter = MountedCharacter;
	}
}

void AAuraBroomVehicle::OnBroomMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsValid(MountedCharacter))
	{
		return;
	}

	ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
	if (!IsValid(HitCharacter))
	{
		return;
	}

	if (IsWithinRemountGraceWindow(HitCharacter))
	{
		UE_LOG(LogAura, Verbose, TEXT("Broom[%s] OnBroomMeshHit ignored due to remount grace window. Character=%s"),
			*GetNameSafe(this),
			*GetNameSafe(HitCharacter));
		return;
	}

	if (HasAuthority())
	{
		UE_LOG(LogAura, Log, TEXT("Broom[%s] OnBroomMeshHit server-authority mount. Character=%s"),
			*GetNameSafe(this),
			*GetNameSafe(HitCharacter));
		MountCharacterInternal(HitCharacter);
		return;
	}

	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(HitCharacter->GetController()))
	{
		if (AuraPC->IsLocalController())
		{
			UE_LOG(LogAura, Log, TEXT("Broom[%s] OnBroomMeshHit client forwarding mount request. Character=%s Controller=%s"),
				*GetNameSafe(this),
				*GetNameSafe(HitCharacter),
				*GetNameSafe(AuraPC));
			AuraPC->RequestBroomMount(this);
		}
	}

}

void AAuraBroomVehicle::MountCharacterInternal(ACharacter* CharacterToMount)
{
	if (!HasAuthority() || !IsValid(CharacterToMount) || MountedCharacter == CharacterToMount)
	{
		return;
	}

	if (IsWithinRemountGraceWindow(CharacterToMount))
	{
		UE_LOG(LogAura, Log, TEXT("Broom[%s] MountCharacterInternal blocked by remount grace window. Character=%s"),
			*GetNameSafe(this),
			*GetNameSafe(CharacterToMount));
		return;
	}

	if (IsValid(MountedCharacter))
	{
		DismountCharacterInternal();
	}

	MountedCharacter = CharacterToMount;
	ApplyMountedState(MountedCharacter, true);
	LastMountedCharacter = MountedCharacter;
}

void AAuraBroomVehicle::DismountCharacterInternal()
{
	if (!HasAuthority() || !IsValid(MountedCharacter))
	{
		UE_LOG(LogAura, Warning, TEXT("Broom[%s] DismountCharacterInternal aborted. HasAuthority=%s MountedCharacterValid=%s"),
			*GetNameSafe(this),
			HasAuthority() ? TEXT("true") : TEXT("false"),
			IsValid(MountedCharacter) ? TEXT("true") : TEXT("false"));
		return;
	}

	ACharacter* PreviouslyMountedCharacter = MountedCharacter;
	FVector DismountLocation = PreviouslyMountedCharacter->GetActorLocation();
	FRotator DismountRotation = PreviouslyMountedCharacter->GetActorRotation();
	bool bUsedDismountSocket = false;
	if (IsValid(BroomMesh) && BroomMesh->DoesSocketExist(DismountSocketName))
	{
		DismountLocation = BroomMesh->GetSocketLocation(DismountSocketName);
		DismountRotation = BroomMesh->GetSocketRotation(DismountSocketName);
		bUsedDismountSocket = true;
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("Broom[%s] Dismount socket '%s' missing or BroomMesh invalid. Falling back to current character transform. BroomMesh=%s"),
			*GetNameSafe(this),
			*DismountSocketName.ToString(),
			*GetNameSafe(BroomMesh));
	}

	UE_LOG(LogAura, Log, TEXT("Broom[%s] Dismounting character %s. UsingSocket=%s TargetLocation=%s TargetRotation=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PreviouslyMountedCharacter),
		bUsedDismountSocket ? TEXT("true") : TEXT("false"),
		*DismountLocation.ToCompactString(),
		*DismountRotation.ToCompactString());

	MountedCharacter = nullptr;
	ApplyMountedState(PreviouslyMountedCharacter, false);
	PreviouslyMountedCharacter->SetActorLocationAndRotation(DismountLocation, DismountRotation, false, nullptr, ETeleportType::TeleportPhysics);
	LastMountedCharacter = nullptr;
	LastDismountedCharacter = PreviouslyMountedCharacter;
	LastDismountServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	UE_LOG(LogAura, Log, TEXT("Broom[%s] Dismount complete. PreviousCharacter=%s NewMountedCharacter=%s"),
		*GetNameSafe(this),
		*GetNameSafe(PreviouslyMountedCharacter),
		*GetNameSafe(MountedCharacter));
}

bool AAuraBroomVehicle::IsWithinRemountGraceWindow(const ACharacter* Character) const
{
	if (!IsValid(Character) || !IsValid(LastDismountedCharacter) || Character != LastDismountedCharacter)
	{
		return false;
	}

	if (RemountGracePeriodSeconds <= 0.f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const float TimeSinceLastDismount = World->GetTimeSeconds() - LastDismountServerTime;
	return TimeSinceLastDismount < RemountGracePeriodSeconds;
}

void AAuraBroomVehicle::ApplyMountedState(ACharacter* Character, bool bIsMounted)
{
	if (!IsValid(Character))
	{
		return;
	}

	// Propagate the mount state to the character so the AnimInstance and any
	// Blueprint logic can read it without polling the attachment hierarchy.
	if (AAuraCharacterBase* AuraCharacter = Cast<AAuraCharacterBase>(Character))
	{
		AuraCharacter->bIsMounted = bIsMounted;
	}

	if (bIsMounted)
	{
		Character->AttachToComponent(BroomMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, RiderSocketName);
		Character->SetActorEnableCollision(false);

		// Stop the character from independently replicating its world position via
		// AActor::ReplicatedMovement. While attached, the character's world position is
		// fully derived each frame from the broom's replicated transform + the socket
		// offset, so a separate ReplicatedMovement packet arriving out-of-sync with the
		// broom's position packet would cause visible desync (OnRep_ReplicatedMovement
		// snapping the character to a stale world position). Disabling it here makes the
		// attachment hierarchy the single source of truth on every client.
		Character->SetReplicateMovement(false);

		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->DisableMovement();
			// Clear any residual velocity so the CMC does not carry momentum into the
			// mounted state or confuse the network prediction pipeline.
			CharacterMovement->StopMovementImmediately();
		}
	}
	else
	{
		Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Character->SetActorEnableCollision(true);

		// Re-enable independent position replication now that the character moves freely.
		Character->SetReplicateMovement(true);

		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling);
			UE_LOG(LogAura, Verbose, TEXT("Broom[%s] ApplyMountedState dismount for %s: MovementMode set to Falling."),
				*GetNameSafe(this),
				*GetNameSafe(Character));
		}
		else
		{
			UE_LOG(LogAura, Warning, TEXT("Broom[%s] ApplyMountedState dismount for %s: CharacterMovement is null."),
				*GetNameSafe(this),
				*GetNameSafe(Character));
		}
	}
}

void AAuraBroomVehicle::SetFlightTargetYaw(float WorldYaw)
{
	FlightTargetYaw = WorldYaw;
	bHasFlightTargetYaw = true;
}

void AAuraBroomVehicle::UpdateFlightYaw(float DeltaSeconds)
{
	// Only the server drives broom rotation; movement replication carries it to clients.
	if (!HasAuthority() || !bHasFlightTargetYaw || !IsValid(MountedCharacter))
	{
		return;
	}

	const FRotator CurrentRot = GetActorRotation();
	const FRotator TargetRot(0.f, FlightTargetYaw, 0.f);

	// RInterpTo handles the shortest-path wrap at the ±180° boundary.
	const FRotator NewRot = FMath::RInterpTo(CurrentRot, TargetRot, DeltaSeconds, YawInterpSpeed);
	SetActorRotation(NewRot);
}

void AAuraBroomVehicle::UpdateIdleHover(float DeltaSeconds)
{
	if (!HasAuthority() || !bEnableIdleHover)
	{
		return;
	}

	const bool bShouldHover = !IsValid(MountedCharacter);
	if (!bShouldHover)
	{
		bWasHoveringLastTick = false;
		IdleHoverCurrentOffset = FVector::ZeroVector;
		IdleHoverCurrentRotationOffset = FRotator::ZeroRotator;
		IdleHoverBaseLocation = GetActorLocation();
		IdleHoverBaseRotation = GetActorRotation();
		return;
	}

	if (!bWasHoveringLastTick)
	{
		IdleHoverBaseLocation = GetActorLocation();
		IdleHoverBaseRotation = GetActorRotation();
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
	SetActorLocationAndRotation(NewLocation, NewRotation);

	bWasHoveringLastTick = true;
}
