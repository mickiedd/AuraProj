// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "BehaviorUAgent.h"
#include "AutoTestRunContext.h"
#include "AutoTestAgent.generated.h"

class AActor;
class ACharacter;

/**
 * UAutoTestAgent — BehaviorU agent subclass used to drive a single test script.
 *
 * Auto-registers with UBehaviorUWorldSubsystem in BeginPlay (via Super), so the tree
 * is ticked by the two-phase subsystem — the runner never ticks it manually.
 * Holds a raw pointer to the per-test FAutoTestRunContext (owned by the runner) so the
 * custom test nodes can record assertions.
 *
 * Registers a generic "SpawnActor" method handler so <SpawnActor> nodes can defer actor
 * spawning to the game thread (Phase 2 worker thread cannot spawn). The node stashes its
 * config into fixed blackboard keys; this handler reads them, spawns, and stores the
 * spawned actor into the requested blackboard object property.
 *
 * Player-drive method handlers (StartAutoRun/StopAutoRun/Jump/Crouch/UnCrouch/
 * RandomJumpOrCrouch) drive the local player pawn's ACharacter API directly. Continuous
 * forward movement is applied every game-thread frame by an FTSTicker while bAutoRunning
 * is set (the BehaviorU subsystem disables this component's own tick, so the ticker is
 * the smooth-movement channel).
 */
UCLASS(ClassGroup = (Testing), meta = (BlueprintSpawnableComponent), DisplayName = "AutoTest Agent")
class AURAAUTOTESTRUNTIME_API UAutoTestAgent : public UBehaviorUAgentComponent
{
	GENERATED_BODY()

public:
	UAutoTestAgent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Bind the per-test results context (called by the runner on the game thread). */
	void SetRunContext(FAutoTestRunContext* InContext) { RunContext = InContext; }
	FAutoTestRunContext* GetRunContext() const { return RunContext; }

	// --- Blackboard key convention used by the SpawnActor node + handler ---
	static const FString& SpawnConfigClassKey();
	static const FString& SpawnConfigLocationKey();
	static const FString& SpawnConfigOutputKey();

protected:
	/** Generic "SpawnActor" method handler: reads the spawn-config blackboard keys set
	 *  by the SpawnActor node, spawns the actor on the game thread, and stores it into
	 *  the OutputKey blackboard object property. */
	EBehaviorUStatus HandleSpawnActor();

	// --- Player-drive method handlers (called from <Action Method="..."> nodes) ---
	EBehaviorUStatus HandleStartAutoRun();       // begin continuous forward movement
	EBehaviorUStatus HandleStopAutoRun();         // stop continuous forward movement
	EBehaviorUStatus HandleJump();                // ACharacter::Jump()
	EBehaviorUStatus HandleStopJump();           // ACharacter::StopJumping()
	EBehaviorUStatus HandleCrouch();              // ACharacter::Crouch()
	EBehaviorUStatus HandleUnCrouch();            // ACharacter::UnCrouch()
	EBehaviorUStatus HandleRandomJumpOrCrouch();  // randomly jump or crouch each call

private:
	/** Raw, non-UPROPERTY pointer; lifetime is owned by the runner and outlives the agent. */
	FAutoTestRunContext* RunContext = nullptr;

	// --- Continuous auto-run movement ---
	bool bAutoRunning = false;
	FTSTicker::FDelegateHandle AutoRunTickHandle;

	/** Find the local player character, if any. */
	ACharacter* GetPlayerCharacter() const;

	/** FTSTicker callback: applies forward movement every game-thread frame while auto-running. */
	bool OnAutoRunTick(float DeltaSeconds);
};