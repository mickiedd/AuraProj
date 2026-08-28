// Copyright Druid Mechanics

#include "Vehicle/AuraBroomMountComponent.h"

#include "Vehicle/AuraBroomVehicle.h"
#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Character/AuraCharacterBase.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/AuraPlayerController.h"

UAuraBroomMountComponent::UAuraBroomMountComponent()
{
	// Use SetIsReplicatedByDefault (not SetIsReplicated) in the ctor: the component
	// isn't registered with its owner yet, so this sets the default applied at
	// registration time, which is UE's preferred constructor-time API.
	SetIsReplicatedByDefault(true);
	bAutoActivate = true;
}

void UAuraBroomMountComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAuraBroomMountComponent, MountedCharacter);
}

AAuraBroomVehicle* UAuraBroomMountComponent::GetBroomOwner() const
{
	return Cast<AAuraBroomVehicle>(GetOwner());
}

void UAuraBroomMountComponent::BeginPlay()
{
	Super::BeginPlay();

	// Bind collision-driven mounting to the broom's mesh. The mesh is created and
	// owned by the broom; the mount component just reacts to its hits.
	AAuraBroomVehicle* Broom = GetBroomOwner();
	if (Broom && Broom->GetBroomMesh())
	{
		Broom->GetBroomMesh()->OnComponentHit.AddDynamic(this, &UAuraBroomMountComponent::OnBroomMeshHit);
	}
}

void UAuraBroomMountComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind to avoid a dangling delegate if the broom outlives this component.
	if (AAuraBroomVehicle* Broom = GetBroomOwner())
	{
		if (Broom->GetBroomMesh())
		{
			Broom->GetBroomMesh()->OnComponentHit.RemoveDynamic(this, &UAuraBroomMountComponent::OnBroomMeshHit);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UAuraBroomMountComponent::RequestMount(ACharacter* CharacterToMount)
{
	if (!IsValid(CharacterToMount))
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		MountCharacterInternal(CharacterToMount);
		return;
	}

	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(CharacterToMount->GetController()); AuraPC && AuraPC->IsLocalController())
	{
		AuraPC->RequestBroomMount(GetBroomOwner());
	}
}

void UAuraBroomMountComponent::RequestDismount()
{
	UE_LOG(LogAura, Log, TEXT("Broom[%s] RequestDismount received. HasAuthority=%s MountedCharacter=%s"),
		*GetNameSafe(GetOwner()),
		GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(MountedCharacter));

	if (GetOwner()->HasAuthority())
	{
		DismountCharacterInternal();
		return;
	}

	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(MountedCharacter ? MountedCharacter->GetController() : nullptr); AuraPC && AuraPC->IsLocalController())
	{
		AuraPC->RequestBroomDismount(GetBroomOwner());
	}
}

void UAuraBroomMountComponent::OnRep_MountedCharacter()
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

void UAuraBroomMountComponent::OnBroomMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
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
			*GetNameSafe(GetOwner()),
			*GetNameSafe(HitCharacter));
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		UE_LOG(LogAura, Log, TEXT("Broom[%s] OnBroomMeshHit server-authority mount. Character=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(HitCharacter));
		MountCharacterInternal(HitCharacter);
		return;
	}

	if (AAuraPlayerController* AuraPC = Cast<AAuraPlayerController>(HitCharacter->GetController()))
	{
		if (AuraPC->IsLocalController())
		{
			UE_LOG(LogAura, Log, TEXT("Broom[%s] OnBroomMeshHit client forwarding mount request. Character=%s Controller=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(HitCharacter),
				*GetNameSafe(AuraPC));
			AuraPC->RequestBroomMount(GetBroomOwner());
		}
	}
}

void UAuraBroomMountComponent::MountCharacterInternal(ACharacter* CharacterToMount)
{
	if (!GetOwner()->HasAuthority() || !IsValid(CharacterToMount) || MountedCharacter == CharacterToMount)
	{
		return;
	}
	if (AActor* ExistingMount = CharacterToMount->GetAttachParentActor(); IsValid(ExistingMount) && ExistingMount != GetOwner())
	{
		UE_LOG(LogAura, Warning, TEXT("Broom[%s] MountCharacterInternal rejected character=%s already attached to %s."),
			*GetNameSafe(GetOwner()), *GetNameSafe(CharacterToMount), *GetNameSafe(ExistingMount));
		return;
	}

	if (IsWithinRemountGraceWindow(CharacterToMount))
	{
		UE_LOG(LogAura, Log, TEXT("Broom[%s] MountCharacterInternal blocked by remount grace window. Character=%s"),
			*GetNameSafe(GetOwner()),
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

	// A rider is taking over — clear any persistent BT follow thrust so it doesn't
	// fight the rider's own AddFlightInput while mounted.
	if (AAuraBroomVehicle* Broom = GetBroomOwner())
	{
		Broom->SetBtFlightThrust(FVector::ZeroVector);
	}
}

void UAuraBroomMountComponent::DismountCharacterInternal()
{
	if (!GetOwner()->HasAuthority() || !IsValid(MountedCharacter))
	{
		UE_LOG(LogAura, Warning, TEXT("Broom[%s] DismountCharacterInternal aborted. HasAuthority=%s MountedCharacterValid=%s"),
			*GetNameSafe(GetOwner()),
			GetOwner()->HasAuthority() ? TEXT("true") : TEXT("false"),
			IsValid(MountedCharacter) ? TEXT("true") : TEXT("false"));
		return;
	}

	ACharacter* PreviouslyMountedCharacter = MountedCharacter;
	FVector DismountLocation = PreviouslyMountedCharacter->GetActorLocation();
	FRotator DismountRotation = PreviouslyMountedCharacter->GetActorRotation();
	bool bUsedDismountSocket = false;
	UStaticMeshComponent* BroomMesh = GetBroomOwner() ? GetBroomOwner()->GetBroomMesh() : nullptr;
	if (IsValid(BroomMesh) && BroomMesh->DoesSocketExist(DismountSocketName))
	{
		DismountLocation = BroomMesh->GetSocketLocation(DismountSocketName);
		DismountRotation = BroomMesh->GetSocketRotation(DismountSocketName);
		bUsedDismountSocket = true;
	}
	else
	{
		UE_LOG(LogAura, Warning, TEXT("Broom[%s] Dismount socket '%s' missing or BroomMesh invalid. Falling back to current character transform. BroomMesh=%s"),
			*GetNameSafe(GetOwner()),
			*DismountSocketName.ToString(),
			*GetNameSafe(BroomMesh));
	}

	UE_LOG(LogAura, Log, TEXT("Broom[%s] Dismounting character %s. UsingSocket=%s TargetLocation=%s TargetRotation=%s"),
		*GetNameSafe(GetOwner()),
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
		*GetNameSafe(GetOwner()),
		*GetNameSafe(PreviouslyMountedCharacter),
		*GetNameSafe(MountedCharacter));
}

bool UAuraBroomMountComponent::IsWithinRemountGraceWindow(const ACharacter* Character) const
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

float UAuraBroomMountComponent::GetTimeSinceLastDismount() const
{
	if (!IsValid(LastDismountedCharacter) || LastDismountServerTime <= 0.f)
	{
		return -1.f;
	}

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return -1.f;
	}

	return World->GetTimeSeconds() - LastDismountServerTime;
}

void UAuraBroomMountComponent::ApplyMountedState(ACharacter* Character, bool bIsMounted)
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

	// Add or remove the gameplay tag to block ability input when mounted
	if (UAuraAbilitySystemComponent* ASC = Cast<UAuraAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Character)))
	{
		if (bIsMounted)
		{
			ASC->AddLooseGameplayTag(FAuraGameplayTags::Get().Player_Mounted_Broom);
			UE_LOG(LogAura, Log, TEXT("Broom[%s] Applied Player_Mounted_Broom tag to %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Character));
		}
		else
		{
			ASC->RemoveLooseGameplayTag(FAuraGameplayTags::Get().Player_Mounted_Broom);
			UE_LOG(LogAura, Log, TEXT("Broom[%s] Removed Player_Mounted_Broom tag from %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Character));
		}
	}

	UStaticMeshComponent* BroomMesh = GetBroomOwner() ? GetBroomOwner()->GetBroomMesh() : nullptr;

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
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Character));
		}
		else
		{
			UE_LOG(LogAura, Warning, TEXT("Broom[%s] ApplyMountedState dismount for %s: CharacterMovement is null."),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Character));
		}
	}
}
