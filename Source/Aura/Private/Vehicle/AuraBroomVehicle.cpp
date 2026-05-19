// Copyright Druid Mechanics


#include "Vehicle/AuraBroomVehicle.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AAuraBroomVehicle::AAuraBroomVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BroomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BroomMesh"));
	BroomMesh->SetupAttachment(Root);
	BroomMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	BroomMesh->SetIsReplicated(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BroomMeshAsset(TEXT("/Game/Assets/Vehicle/Mesh/Broom.Broom"));
	if (BroomMeshAsset.Succeeded())
	{
		BroomMesh->SetStaticMesh(BroomMeshAsset.Object);
	}

	FlightMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FlightMovement"));
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
}

void AAuraBroomVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAuraBroomVehicle, MountedCharacter);
}

void AAuraBroomVehicle::AddFlightInput(const FVector& WorldDirection, float ScaleValue)
{
	AddMovementInput(WorldDirection, ScaleValue);
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

void AAuraBroomVehicle::MountCharacterInternal(ACharacter* CharacterToMount)
{
	if (!HasAuthority() || !IsValid(CharacterToMount) || MountedCharacter == CharacterToMount)
	{
		return;
	}

	if (IsValid(MountedCharacter))
	{
		DismountCharacterInternal();
	}

	MountedCharacter = CharacterToMount;
	ApplyMountedState(MountedCharacter, true);
	LastMountedCharacter = MountedCharacter;

	if (bPossessOnMount)
	{
		if (AController* RiderController = CharacterToMount->GetController())
		{
			RiderController->Possess(this);
		}
	}
}

void AAuraBroomVehicle::DismountCharacterInternal()
{
	if (!HasAuthority() || !IsValid(MountedCharacter))
	{
		return;
	}

	ACharacter* PreviouslyMountedCharacter = MountedCharacter;

	if (AController* CurrentController = GetController())
	{
		if (bRestoreCharacterControlOnDismount)
		{
			CurrentController->Possess(PreviouslyMountedCharacter);
		}
		else
		{
			CurrentController->UnPossess();
		}
	}

	MountedCharacter = nullptr;
	ApplyMountedState(PreviouslyMountedCharacter, false);
	LastMountedCharacter = nullptr;
}

void AAuraBroomVehicle::ApplyMountedState(ACharacter* Character, bool bIsMounted)
{
	if (!IsValid(Character))
	{
		return;
	}

	if (bIsMounted)
	{
		Character->AttachToComponent(BroomMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, RiderSocketName);
		Character->SetActorEnableCollision(false);
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->DisableMovement();
		}
	}
	else
	{
		Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Character->SetActorEnableCollision(true);
		if (UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement())
		{
			CharacterMovement->SetMovementMode(EMovementMode::MOVE_Walking);
		}
	}
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
