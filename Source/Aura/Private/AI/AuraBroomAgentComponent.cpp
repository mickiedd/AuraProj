// Copyright Druid Mechanics

#include "AI/AuraBroomAgentComponent.h"
#include "Vehicle/AuraBroomVehicle.h"
#include "Character/AuraCharacter.h"
#include "Aura/AuraLogChannels.h"
#include "BehaviacTypes.h"
#include "Kismet/GameplayStatics.h"

UAuraBroomAgentComponent::UAuraBroomAgentComponent()
{
	// Auto-load the broom follow tree shipped in Content/BehaviorTrees/.
	AutoLoadXMLFilePath = TEXT("/Game/BehaviorTrees/BT_BroomFollowPlayer.xml");
	bAutoTick = true;
	FollowSpeedScale = 1.0f;
	bStopFollowWhenMounted = true;
}

void UAuraBroomAgentComponent::BeginPlay()
{
	// The broom BT is server-authoritative: movement is replicated from the
	// server to clients, so the tree only needs to run on the server. Skip
	// auto-load, method registration, and blackboard seeding on clients.
	const bool bIsServer = GetOwner() && GetOwner()->HasAuthority();

	if (!bIsServer)
	{
		// Prevent Super::BeginPlay from auto-loading the tree on clients.
		AutoLoadXMLFilePath.Empty();
		bAutoTick = false;
	}

	// Super::BeginPlay registers with the Behaviac world subsystem and, on the
	// server, auto-loads the broom follow tree because AutoLoadXMLFilePath is set.
	Super::BeginPlay();

	if (!bIsServer)
	{
		// Nothing else to do on clients — no tree, no ticking, no handlers.
		return;
	}

	// Seed the blackboard keys that BT_BroomFollowPlayer.xml reads and writes.
	SetBoolProperty(TEXT("bPlayerInScene"), false);
	SetVectorProperty(TEXT("PlayerLocation"), FVector::ZeroVector);

	// Bind the C++ behavior methods referenced by the tree's <Action> nodes.
	RegisterMethodHandler(TEXT("FindPlayer"),   [this]() { return Method_FindPlayer();   });
	RegisterMethodHandler(TEXT("FollowPlayer"),  [this]() { return Method_FollowPlayer();  });

	UE_LOG(LogAura, Log, TEXT("[BroomBehaviac] Agent bound to %s (server), auto-loading %s"),
		*GetNameSafe(GetOwner()), *AutoLoadXMLFilePath);
}

AAuraBroomVehicle* UAuraBroomAgentComponent::GetBroomOwner() const
{
	return Cast<AAuraBroomVehicle>(GetOwner());
}

EBehaviacStatus UAuraBroomAgentComponent::Method_FindPlayer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return EBehaviacStatus::Failure;
	}

	// Find the first player character in the scene.
	// We use UGameplayStatics::GetActorOfClass which iterates the world for an
	// actor of the specified class — simple and reliable for a single-player scene.
	ACharacter* PlayerChar = UGameplayStatics::GetPlayerCharacter(World, 0);
	if (!PlayerChar)
	{
		// Fallback: search for any AAuraCharacter in the world.
		TArray<AActor*> FoundCharacters;
		UGameplayStatics::GetAllActorsOfClass(World, AAuraCharacter::StaticClass(), FoundCharacters);
		if (FoundCharacters.Num() > 0)
		{
			PlayerChar = Cast<ACharacter>(FoundCharacters[0]);
		}
	}

	if (!PlayerChar)
	{
		SetBoolProperty(TEXT("bPlayerInScene"), false);
		return EBehaviacStatus::Success;
	}

	// Store the player's location in the blackboard.
	const FVector PlayerLocation = PlayerChar->GetActorLocation();
	SetVectorProperty(TEXT("PlayerLocation"), PlayerLocation);
	SetBoolProperty(TEXT("bPlayerInScene"), true);

	return EBehaviacStatus::Success;
}

EBehaviacStatus UAuraBroomAgentComponent::Method_FollowPlayer()
{
	AAuraBroomVehicle* Broom = GetBroomOwner();
	if (!Broom)
	{
		return EBehaviacStatus::Failure;
	}

	// If the broom currently has a rider, stop following — the player is
	// controlling it manually and the BT should not interfere.
	if (bStopFollowWhenMounted && Broom->GetMountedCharacter())
	{
		return EBehaviacStatus::Success;
	}

	// The tree only runs on the server (BeginPlay skips auto-load on clients),
	// but guard against any edge case where a client might still tick.
	if (!Broom->HasAuthority())
	{
		return EBehaviacStatus::Success;
	}

	const FVector PlayerLocation = GetVectorProperty(TEXT("PlayerLocation"));
	if (PlayerLocation.IsNearlyZero(1.f))
	{
		return EBehaviacStatus::Success;
	}

	const FVector BroomLocation = Broom->GetActorLocation();
	const FVector Direction = PlayerLocation - BroomLocation;

	// If we're close enough, don't move — prevents jittering on top of the player.
	const float Distance = Direction.Size();
	const float StopRadius = 150.f;
	if (Distance <= StopRadius)
	{
		return EBehaviacStatus::Success;
	}

	// Normalize and apply flight input toward the player.
	const FVector NormalizedDir = Direction / Distance;
	Broom->AddFlightInput(NormalizedDir, FollowSpeedScale);

	// Update the broom's facing to point toward the player.
	Broom->SetFlightTargetYaw(FRotationMatrix::MakeFromX(NormalizedDir).Rotator().Yaw);

	BEHAVIAC_VLOG(TEXT("[BroomBehaviac] FollowPlayer: dist=%.0f dir=%s"),
		Distance, *NormalizedDir.ToCompactString());

	return EBehaviacStatus::Success;
}