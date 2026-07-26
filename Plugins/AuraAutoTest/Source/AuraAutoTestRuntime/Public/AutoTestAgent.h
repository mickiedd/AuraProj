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
class UCharacterMovementComponent;

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
 * RandomJumpOrCrouch) drive the local player pawn's ACharacter API directly. While
 * bAutoRunning is set, an FTSTicker applies natural wander movement every game-thread
 * frame (the BehaviorU subsystem disables this component's own tick, so the ticker is
 * the smooth-movement channel): a world-space heading that smoothly curves left/right,
 * varied stride intensity, occasional idle pauses, and periodic sprint bursts — so the
 * player walks forward, veers into other directions, and sometimes runs, instead of
 * marching in a straight line forever.
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
	EBehaviorUStatus HandleStartAutoRun();       // begin natural wander movement
	EBehaviorUStatus HandleStopAutoRun();         // stop wander movement, restore walk speed
	EBehaviorUStatus HandleJump();                // ACharacter::Jump()
	EBehaviorUStatus HandleStopJump();           // ACharacter::StopJumping()
	EBehaviorUStatus HandleCrouch();              // ACharacter::Crouch()
	EBehaviorUStatus HandleUnCrouch();            // ACharacter::UnCrouch()
	EBehaviorUStatus HandleRandomJumpOrCrouch();  // jump / crouch / nothing each call
	EBehaviorUStatus HandleUseRandomSkill();      // fire a random equipped ability (sometimes)
	EBehaviorUStatus HandleAssertAbilityGraphLoaded();

	/** Apply or clear the auto-run sprint state on the controlled character's movement
	 *  component (raises MaxWalkSpeed by AutoRunSprintMultiplier while sprinting). */
	void ApplyAutoRunSprint(UCharacterMovementComponent* MoveComp);

	/** Restore the controlled character's base walk speed and clear cached sprint state.
	 *  Called on StopAutoRun and EndPlay so a finished/aborted test never leaves the pawn
	 *  sprinting. */
	void RestoreAutoRunWalkSpeed();

private:
	/** Raw, non-UPROPERTY pointer; lifetime is owned by the runner and outlives the agent. */
	FAutoTestRunContext* RunContext = nullptr;

	// --- Continuous auto-run movement (natural wander) ---
	bool bAutoRunning = false;
	FTSTicker::FDelegateHandle AutoRunTickHandle;

	// Wander steering: a world-space heading (yaw, degrees) that smoothly curves over
	// time so the pawn veers into different directions instead of marching straight.
	float AutoRunHeadingYaw = 0.f;       // current movement heading
	float AutoRunAngularVel = 0.f;       // current turn rate (deg/sec), smoothed
	float AutoRunTargetAngularVel = 0.f; // turn rate we are easing toward
	float AutoRunWanderTimer = 0.f;      // retarget countdown for the turn rate

	// Stride intensity: varies how hard we lean into AddMovementInput (0..1).
	float AutoRunIntentScale = 1.f;
	float AutoRunIntentTimer = 0.f;

	// Sprint bursts: periodically toggle a run, raising MaxWalkSpeed while active.
	bool bAutoRunSprinting = false;
	float AutoRunSprintTimer = 0.f;
	float AutoRunBaseWalkSpeed = 0.f;    // cached walk speed to restore after sprinting

	// Idle pauses: occasionally stand still for a beat, then walk on.
	bool bAutoRunPaused = false;
	float AutoRunPauseTimer = 0.f;

	/** Find the local player character, if any. */
	ACharacter* GetPlayerCharacter() const;

	/** FTSTicker callback: applies natural wander movement every game-thread frame while
	 *  auto-running — curved heading, varied stride, idle pauses, and sprint bursts. */
	bool OnAutoRunTick(float DeltaSeconds);
};