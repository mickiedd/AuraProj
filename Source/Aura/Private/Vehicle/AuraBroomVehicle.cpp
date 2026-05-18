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
	PrimaryActorTick.bCanEverTick = false;
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
