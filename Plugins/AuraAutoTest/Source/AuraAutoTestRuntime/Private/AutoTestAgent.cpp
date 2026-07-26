// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestAgent.h"
#include "AutoTestLog.h"
#include "BehaviorUTypes.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"

static const FString GSpawnClassKey		= TEXT("__AutoTest_SpawnClass");
static const FString GSpawnLocationKey	= TEXT("__AutoTest_SpawnLocation");
static const FString GSpawnOutputKey	= TEXT("__AutoTest_SpawnOutput");

// --- Natural auto-run wander tuning ---
// Matches AAuraPlayerController::SprintSpeedMultiplier so a sprint burst here runs at the
// same speed a real player reaches by holding Shift.
static constexpr float GAutoRunSprintMultiplier = 1.5f;
// Peak turn rate (deg/sec). Wander retargets pick a target in [-Max, +Max] every ~0.8-2.5s
// and we ease toward it, so most of the time the pawn traces gentle arcs rather than spinning.
static constexpr float GAutoRunMaxTurnRate = 110.f;

UAutoTestAgent::UAutoTestAgent()
{
	bAutoTick = true;
	// AutoLoadXMLFilePath is left empty: the runner loads the test tree explicitly
	// via LoadBehaviorTreeFromXMLFile after wiring the run context.
}

void UAutoTestAgent::BeginPlay()
{
	// Super auto-registers with UBehaviorUWorldSubsystem (bManagedBySubsystem = true,
	// component tick disabled) â€?the subsystem drives the two-phase tick. We must NOT
	// tick the tree manually.
	Super::BeginPlay();

	RegisterMethodHandler(TEXT("SpawnActor"),         [this]() { return HandleSpawnActor(); });
	RegisterMethodHandler(TEXT("StartAutoRun"),        [this]() { return HandleStartAutoRun(); });
	RegisterMethodHandler(TEXT("StopAutoRun"),         [this]() { return HandleStopAutoRun(); });
	RegisterMethodHandler(TEXT("Jump"),                [this]() { return HandleJump(); });
	RegisterMethodHandler(TEXT("StopJump"),           [this]() { return HandleStopJump(); });
	RegisterMethodHandler(TEXT("Crouch"),              [this]() { return HandleCrouch(); });
	RegisterMethodHandler(TEXT("UnCrouch"),            [this]() { HandleUnCrouch(); return EBehaviorUStatus::Success; });
	RegisterMethodHandler(TEXT("RandomJumpOrCrouch"), [this]() { return HandleRandomJumpOrCrouch(); });
	RegisterMethodHandler(TEXT("UseRandomSkill"),      [this]() { return HandleUseRandomSkill(); });
	RegisterMethodHandler(TEXT("AssertAbilityGraphLoaded"), [this]() { return HandleAssertAbilityGraphLoaded(); });

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
	// StopRun aborts the tree mid-loop, so the BT's trailing StopAutoRun is never reached.
	// Make sure we never leave the pawn sprinting after the test host is torn down.
	RestoreAutoRunWalkSpeed();

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

	// Seed the wander heading from the controller's current facing so the pawn starts by
	// walking forward, then let the steering ease it into curves over time.
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			AutoRunHeadingYaw = PC->GetControlRotation().Yaw;
		}
	}

	// Reset the wander/sprint/pause state so each run begins from a clean walk.
	AutoRunAngularVel = 0.f;
	AutoRunTargetAngularVel = 0.f;
	AutoRunWanderTimer = 0.f;
	AutoRunIntentScale = 1.f;
	AutoRunIntentTimer = 0.f;
	bAutoRunSprinting = false;
	AutoRunSprintTimer = FMath::FRandRange(2.f, 5.f); // walk a stretch before the first sprint
	AutoRunBaseWalkSpeed = 0.f;
	bAutoRunPaused = false;
	AutoRunPauseTimer = FMath::FRandRange(3.f, 6.5f);

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] StartAutoRun: natural wander movement enabled (heading=%.1f)."), AutoRunHeadingYaw);
	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UAutoTestAgent::HandleStopAutoRun()
{
	bAutoRunning = false;
	RestoreAutoRunWalkSpeed();
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] StopAutoRun: wander movement disabled, walk speed restored."));
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
	// Most beats do nothing â€?the pawn just keeps walking/running â€?and only occasionally
	// throws in a jump or a crouch, which reads far more naturally than hopping every cycle.
	const float R = FMath::FRand();
	if (R < 0.2f)
	{
		return HandleJump();   // 20% jump
	}
	if (R < 0.4f)
	{
		return HandleCrouch(); // 20% crouch
	}
	return EBehaviorUStatus::Success; // 60% nothing
}

EBehaviorUStatus UAutoTestAgent::HandleUseRandomSkill()
{
	// Only fire roughly every other beat so the pawn "sometimes" uses a skill rather than
	// spamming it every loop. The controller hook itself no-ops when nothing is equipped or
	// the ability is on cooldown, so this stays gentle either way.
	if (FMath::FRand() >= 0.5f)
	{
		return EBehaviorUStatus::Success;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return EBehaviorUStatus::Failure;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return EBehaviorUStatus::Failure;
	}

	// Invoke the controller's AutoTestUseRandomEquippedAbility hook by name via reflection,
	// so this plugin never has to include or link against Aura. The function takes no args
	// and returns void, so the Parameter buffer is unused.
	static const FName SkillHookName = TEXT("AutoTestUseRandomEquippedAbility");
	if (UFunction* SkillFn = PC->FindFunction(SkillHookName))
	{
		PC->ProcessEvent(SkillFn, nullptr);
		return EBehaviorUStatus::Success;
	}

	UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] UseRandomSkill: controller has no AutoTestUseRandomEquippedAbility hook."));
	return EBehaviorUStatus::Failure;
}

// ===================================================================
// Continuous auto-run movement tick (game thread)
// ===================================================================

void UAutoTestAgent::ApplyAutoRunSprint(UCharacterMovementComponent* MoveComp)
{
	if (!MoveComp)
	{
		return;
	}

	if (bAutoRunSprinting)
	{
		// Cache the base walk speed the first time we sprint this run so we can restore it.
		if (AutoRunBaseWalkSpeed <= 0.f)
		{
			AutoRunBaseWalkSpeed = MoveComp->MaxWalkSpeed;
		}
		const float Target = AutoRunBaseWalkSpeed * GAutoRunSprintMultiplier;
		if (!FMath::IsNearlyEqual(MoveComp->MaxWalkSpeed, Target))
		{
			MoveComp->MaxWalkSpeed = Target;
		}
	}
	else if (AutoRunBaseWalkSpeed > 0.f)
	{
		if (!FMath::IsNearlyEqual(MoveComp->MaxWalkSpeed, AutoRunBaseWalkSpeed))
		{
			MoveComp->MaxWalkSpeed = AutoRunBaseWalkSpeed;
		}
	}
}

void UAutoTestAgent::RestoreAutoRunWalkSpeed()
{
	if (AutoRunBaseWalkSpeed > 0.f)
	{
		if (ACharacter* Char = GetPlayerCharacter())
		{
			if (UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement())
			{
				if (!FMath::IsNearlyEqual(MoveComp->MaxWalkSpeed, AutoRunBaseWalkSpeed))
				{
					MoveComp->MaxWalkSpeed = AutoRunBaseWalkSpeed;
				}
			}
		}
	}
	bAutoRunSprinting = false;
	AutoRunBaseWalkSpeed = 0.f;
}

bool UAutoTestAgent::OnAutoRunTick(float DeltaSeconds)
{
	if (!bAutoRunning)
	{
		return true; // keep the ticker alive; cheap while idle
	}

	ACharacter* Char = GetPlayerCharacter();
	if (!Char)
	{
		return true;
	}
	UCharacterMovementComponent* MoveComp = Char->GetCharacterMovement();
	if (!MoveComp)
	{
		return true;
	}

	// --- Idle beats: occasionally stand still for a moment, then walk on. ---
	AutoRunPauseTimer -= DeltaSeconds;
	if (AutoRunPauseTimer <= 0.f)
	{
		if (bAutoRunPaused)
		{
			bAutoRunPaused = false;
			AutoRunPauseTimer = FMath::FRandRange(3.f, 6.5f);
		}
		else
		{
			bAutoRunPaused = (FMath::FRand() < 0.18f);
			AutoRunPauseTimer = bAutoRunPaused ? FMath::FRandRange(0.4f, 1.2f)
											   : FMath::FRandRange(3.f, 6.5f);
		}
	}

	// --- Sprint beats: toggle run on/off so the pawn sometimes runs. Never start a
	//     sprint while paused; instead push the next decision out. ---
	AutoRunSprintTimer -= DeltaSeconds;
	if (AutoRunSprintTimer <= 0.f)
	{
		if (bAutoRunSprinting)
		{
			bAutoRunSprinting = false;
			AutoRunSprintTimer = FMath::FRandRange(3.0f, 7.5f); // walk gap
		}
		else if (!bAutoRunPaused)
		{
			bAutoRunSprinting = true;
			AutoRunSprintTimer = FMath::FRandRange(1.5f, 4.0f); // sprint burst
		}
		else
		{
			AutoRunSprintTimer = FMath::FRandRange(1.0f, 2.5f); // retry after the pause
		}
		ApplyAutoRunSprint(MoveComp);
	}

	// --- Wander steering: pick a new target turn rate every so often and ease toward
	//     it, so the heading traces gentle arcs and the pawn veers into different
	//     directions instead of marching in a straight line. ---
	AutoRunWanderTimer -= DeltaSeconds;
	if (AutoRunWanderTimer <= 0.f)
	{
		// Centered on 0 (straight-ish); occasionally commit to a harder turn.
		AutoRunTargetAngularVel = (FMath::FRand() - 0.5f) * 2.f * GAutoRunMaxTurnRate;
		if (FMath::FRand() < 0.25f)
		{
			AutoRunTargetAngularVel *= 2.0f;
		}
		AutoRunWanderTimer = FMath::FRandRange(0.8f, 2.5f);
	}
	AutoRunAngularVel = FMath::FInterpTo(AutoRunAngularVel, AutoRunTargetAngularVel, DeltaSeconds, 3.0f);
	AutoRunHeadingYaw = FMath::UnwindDegrees(AutoRunHeadingYaw + AutoRunAngularVel * DeltaSeconds);

	// --- Stride intensity: vary how hard we lean into the input. Full tilt while
	//     sprinting; a relaxed dawdle..brisk walk otherwise. ---
	AutoRunIntentTimer -= DeltaSeconds;
	if (AutoRunIntentTimer <= 0.f)
	{
		AutoRunIntentScale = bAutoRunSprinting ? 1.0f : FMath::FRandRange(0.55f, 1.0f);
		AutoRunIntentTimer = FMath::FRandRange(1.5f, 4.0f);
	}

	// --- Apply movement. AddMovementInput takes a world-space direction; with
	//     bOrientRotationToMovement=true the pawn turns to face it automatically. ---
	const float Scale = bAutoRunPaused ? 0.f : AutoRunIntentScale;
 	if (Scale > KINDA_SMALL_NUMBER)
 	{
 		const FVector MoveDir = FRotationMatrix(FRotator(0.f, AutoRunHeadingYaw, 0.f)).GetUnitAxis(EAxis::X);
 		Char->AddMovementInput(MoveDir, Scale);
 	}
 	return true;
 }

 EBehaviorUStatus UAutoTestAgent::HandleAssertAbilityGraphLoaded()
 {
 	bool bFound = false;
 #if !IS_MONOLITHIC
 	FModuleManager& ModuleManager = FModuleManager::Get();
 	if (ModuleManager.IsModuleLoaded(TEXT("AuraAbilityGraph")))
 	{
 		bFound = true;
 	}
	else if (ModuleManager.LoadModule(TEXT("AuraAbilityGraph")) != nullptr)
 	{
 		bFound = true;
 	}
 #endif
 
 	SetPropertyValue(TEXT("Self.AbilityGraphLoaded"), bFound ? TEXT("true") : TEXT("false"));
 
 	if (!bFound)
 	{
 		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] AssertAbilityGraphLoaded: AuraAbilityGraph module is not loaded."));
 		return EBehaviorUStatus::Failure;
 	}
 
 	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] AssertAbilityGraphLoaded: AuraAbilityGraph module is present."));
 	return EBehaviorUStatus::Success;
 }
