// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestAgent.h"
#include "AutoTestLog.h"
#include "BehaviorUTypes.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectGlobals.h"

static const FString GSpawnClassKey		= TEXT("__AutoTest_SpawnClass");
static const FString GSpawnLocationKey	= TEXT("__AutoTest_SpawnLocation");
static const FString GSpawnOutputKey	= TEXT("__AutoTest_SpawnOutput");

UAutoTestAgent::UAutoTestAgent()
{
	bAutoTick = true;
	// AutoLoadXMLFilePath is left empty: the runner loads the test tree explicitly
	// via LoadBehaviorTreeFromXMLFile after wiring the run context.
}

void UAutoTestAgent::BeginPlay()
{
	// Super auto-registers with UBehaviorUWorldSubsystem (bManagedBySubsystem = true,
	// component tick disabled) — the subsystem drives the two-phase tick. We must NOT
	// tick the tree manually.
	Super::BeginPlay();

	RegisterMethodHandler(TEXT("SpawnActor"),         [this]() { return HandleSpawnActor(); });
	RegisterMethodHandler(TEXT("StartAutoRun"),        [this]() { return HandleStartAutoRun(); });
	RegisterMethodHandler(TEXT("StopAutoRun"),         [this]() { return HandleStopAutoRun(); });
	RegisterMethodHandler(TEXT("Jump"),                [this]() { return HandleJump(); });
	RegisterMethodHandler(TEXT("StopJump"),           [this]() { return HandleStopJump(); });
	RegisterMethodHandler(TEXT("Crouch"),              [this]() { return HandleCrouch(); });
	RegisterMethodHandler(TEXT("UnCrouch"),            [this]() { return HandleUnCrouch(); });
	RegisterMethodHandler(TEXT("RandomJumpOrCrouch"), [this]() { return HandleRandomJumpOrCrouch(); });

	// Continuous-movement ticker (game thread). Cheap when idle; only applies input
	// while bAutoRunning. Removed in EndPlay.
	AutoRunTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UAutoTestAgent::OnAutoRunTick),
		0.0f);
}

void UAutoTestAgent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AutoRunTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(AutoRunTickHandle);
		AutoRunTickHandle.Reset();
	}
	bAutoRunning = false;

	Super::EndPlay(EndPlayReason);
}

const FString& UAutoTestAgent::SpawnConfigClassKey()		{ return GSpawnClassKey; }
const FString& UAutoTestAgent::SpawnConfigLocationKey()	{ return GSpawnLocationKey; }
const FString& UAutoTestAgent::SpawnConfigOutputKey()	{ return GSpawnOutputKey; }

ACharacter* UAutoTestAgent::GetPlayerCharacter() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		return Cast<ACharacter>(PC->GetPawn());
	}
	return nullptr;
}

// ===================================================================
// SpawnActor handler
// ===================================================================

EBehaviorUStatus UAutoTestAgent::HandleSpawnActor()
{
	// Runs on the game thread (Phase 1 command queue). Reads the spawn config the
	// SpawnActor node stashed into the blackboard, loads the class, spawns, and stores
	// the actor into the requested blackboard object property.
	const FString ClassPath = GetPropertyValue(GSpawnClassKey);
	const FString LocationRaw = GetPropertyValue(GSpawnLocationKey);
	const FString OutputKey = GetPropertyValue(GSpawnOutputKey);

	if (ClassPath.IsEmpty())
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] SpawnActor: Class path is empty"));
		return EBehaviorUStatus::Failure;
	}

	UClass* ActorClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (!ActorClass)
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] SpawnActor: failed to load class '%s'"), *ClassPath);
		return EBehaviorUStatus::Failure;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] SpawnActor: no world"));
		return EBehaviorUStatus::Failure;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (!LocationRaw.IsEmpty())
	{
		// Resolve a Self.X reference to its vector string, then parse.
		FString Resolved = LocationRaw;
		if (Resolved.StartsWith(TEXT("Self.")))
		{
			Resolved = GetPropertyValue(Resolved);
		}
		SpawnLocation.InitFromString(Resolved);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	FRotator Rotation = FRotator::ZeroRotator;
	AActor* Spawned = World->SpawnActor(ActorClass, &SpawnLocation, &Rotation, Params);
	if (!Spawned)
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] SpawnActor: SpawnActor returned null for '%s'"), *ClassPath);
		return EBehaviorUStatus::Failure;
	}

	// Store the spawned actor into the requested blackboard object property (GC-protected).
	if (!OutputKey.IsEmpty())
	{
		SetObjectProperty(OutputKey, Spawned);
	}

	// Record for cleanup by the runner.
	if (RunContext)
	{
		RunContext->AddSpawnedActor(Spawned);
	}

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] SpawnActor: spawned '%s' at %s (key='%s')"),
		*Spawned->GetName(), *SpawnLocation.ToCompactString(), *OutputKey);

	return EBehaviorUStatus::Success;
}

// ===================================================================
// Player-drive handlers
// ===================================================================

EBehaviorUStatus UAutoTestAgent::HandleStartAutoRun()
{
	bAutoRunning = true;
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] StartAutoRun: continuous forward movement enabled."));
	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAutoTestAgent::HandleStopAutoRun()
{
	bAutoRunning = false;
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] StopAutoRun: continuous forward movement disabled."));
	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAutoTestAgent::HandleJump()
{
	if (ACharacter* Char = GetPlayerCharacter())
	{
		Char->Jump();
		UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Jump"));
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Failure;
}

EBehaviorUStatus UAutoTestAgent::HandleStopJump()
{
	if (ACharacter* Char = GetPlayerCharacter())
	{
		Char->StopJumping();
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Failure;
}

EBehaviorUStatus UAutoTestAgent::HandleCrouch()
{
	if (ACharacter* Char = GetPlayerCharacter())
	{
		Char->Crouch();
		UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Crouch"));
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Failure;
}

EBehaviorUStatus UAutoTestAgent::HandleUnCrouch()
{
	if (ACharacter* Char = GetPlayerCharacter())
	{
		Char->UnCrouch();
		UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] UnCrouch"));
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Failure;
}

EBehaviorUStatus UAutoTestAgent::HandleRandomJumpOrCrouch()
{
	// 50/50 pick between a jump or a crouch each call.
	const bool bJump = FMath::FRand() < 0.5;
	if (bJump)
	{
		return HandleJump();
	}
	return HandleCrouch();
}

// ===================================================================
// Continuous auto-run movement tick (game thread)
// ===================================================================

bool UAutoTestAgent::OnAutoRunTick(float DeltaSeconds)
{
	if (!bAutoRunning)
	{
		return true; // keep the ticker alive; cheap while idle
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return true;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return true;
	}

	// Forward direction from the controller yaw — the same source the player's Move()
	// handler uses (AAuraPlayerController::Move). bOrientRotationToMovement will rotate
	// the pawn to face this direction automatically.
	const FRotator YawRot(0.f, PC->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDir = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);

	Pawn->AddMovementInput(ForwardDir, 1.0f);
	return true;
}