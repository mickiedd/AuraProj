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

	// The tree only runs on the server (BeginPlay skips auto-load on clients),
	// but guard against any edge case where a client might still tick.
	if (!Broom->HasAuthority())
	{
		return EBehaviacStatus::Success;
	}

	// Per-tick steering is owned by the flight driver (UAuraBroomFlightDriverComponent),
	// which ticks every server tick and feeds the follow component's ComputeFollowThrust
	// the player's LIVE location (60+ Hz). The Behaviac subsystem only ticks this tree
	// at ~10 Hz, so writing the thrust here would reintroduce a ~100ms-stale steering
	// vector once per BT tick and make the follow laggy/weavy. This action is
	// intentionally a no-op success — the BT's job is player discovery + in-scene
	// gating (FindPlayer sets
	// bPlayerInScene, which the precondition above checks), not per-tick steering.
	return EBehaviacStatus::Success;
}