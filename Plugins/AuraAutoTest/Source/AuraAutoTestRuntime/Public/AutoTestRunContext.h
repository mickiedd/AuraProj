// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include <atomic>
#include "AutoTestRunContext.generated.h"

/**
 * Per-test result state, written by test nodes during the BehaviorU worker-thread
 * tick (Phase 2) and read by the runner on the game thread after the tick completes.
 *
 * Not a UObject: it is plain data owned by the runner and handed to the test agent /
 * nodes via raw pointer. All mutable access is lock-protected so the cross-thread
 * handoff is safe.
 */

class AActor;

/** Terminal status of a single test. */
UENUM(BlueprintType)
enum class EAutoTestStatus : uint8
{
	NotRun	UMETA(DisplayName = "NotRun"),
	Running UMETA(DisplayName = "Running"),
	Pass	UMETA(DisplayName = "Pass"),
	Fail	UMETA(DisplayName = "Fail"),
	Timeout UMETA(DisplayName = "Timeout"),
	Error	UMETA(DisplayName = "Error"),
};

/** A single recorded assertion / checkpoint. */
USTRUCT(BlueprintType)
struct FAutoTestAssertion
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	FString Message;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	bool bPassed = false;

	UPROPERTY(VisibleAnywhere, Category = "AutoTest")
	double Timestamp = 0.0; // world time when recorded
};

/**
 * Thread-safe per-test context. Nodes call AddAssertion / SetTerminal from the worker
 * thread; the runner reads Assertions and Status from the game thread after the tree
 * tick completes for the frame.
 */
struct FAutoTestRunContext
{
	/** Append an assertion result (lock-protected). */
	void AddAssertion(FString Message, bool bPassed);

	/**
	 * Move to a terminal status only if still Running (lock-free for the common
	 * "already terminal" check; the set itself is atomic).
	 * @return true if this call performed the transition.
	 */
	bool SetTerminal(EAutoTestStatus NewStatus);

	/** True once a terminal status (Pass/Fail/Timeout/Error) has been reached. */
	bool IsTerminal() const { return Status.load(std::memory_order_relaxed) != EAutoTestStatus::Running; }

	/** Current status (Running until terminal). */
	EAutoTestStatus GetStatus() const { return Status.load(std::memory_order_relaxed); }

	/** Snapshot the assertions array (lock-protected copy). */
	TArray<FAutoTestAssertion> GetAssertions() const;

	/** Snapshot the list of actors spawned by the test, for cleanup. */
	TArray<TWeakObjectPtr<AActor>> GetSpawnedActors() const;

	/** Record an actor spawned during the test so the runner can destroy it. */
	void AddSpawnedActor(AActor* Actor);

	// Test identity (set once by the runner on the game thread before the run).
	FString TestName;
	FString FilePath;

	// Timing (set/updated by the runner on the game thread).
	std::atomic<double> StartTime{0.0};
	std::atomic<double> EndTime{0.0};

private:
	mutable FRWLock Lock;
	TArray<FAutoTestAssertion> Assertions;
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
	std::atomic<EAutoTestStatus> Status{EAutoTestStatus::Running};
};