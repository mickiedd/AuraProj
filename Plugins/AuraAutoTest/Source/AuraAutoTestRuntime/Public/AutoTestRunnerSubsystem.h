// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AutoTestResults.h"
#include "AutoTestRunContext.h"
#include "AutoTestRunnerSubsystem.generated.h"

class UAutoTestAgent;
class AActor;

// Broadcast when a single test finishes. The editor panel subscribes to refresh.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAutoTestTestComplete, const FAutoTestResult&);
// Broadcast when an entire suite run finishes (JSON written).
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAutoTestRunComplete, const FAutoTestSuiteResult&);
// Broadcast when the panel should be opened (raised by the AutoTest.OpenPanel console
// command; the editor module binds and opens the Slate tab — keeps Slate out of runtime).
DECLARE_MULTICAST_DELEGATE(FOnAutoTestRequestOpenPanel);
// Broadcast after a discovery refresh so the panel can repopulate.
DECLARE_MULTICAST_DELEGATE(FOnAutoTestDiscoveryChanged);

/**
 * UAutoTestRunnerSubsystem — discovers BehaviorU test scripts, executes them one at a
 * time against the live (PIE) world, collects results, and writes a JSON report.
 *
 * A UGameInstanceSubsystem so it persists across PIE restarts and is reachable from both
 * console commands and the editor panel. The runner itself is not tickable; it drives
 * per-test watchdog/completion via an FTicker delegate. Test trees are ticked by
 * UBehaviorUWorldSubsystem's two-phase tick (the test agent auto-registers there).
 */
UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestRunnerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --- UGameInstanceSubsystem ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- Discovery ---
	/** Discover all test scripts (plugin Content/AutoTests + project Content/AutoTests). */
	TArray<FAutoTestInfo> DiscoverTests() const;

	/** Force a fresh scan; returns the count and broadcasts OnAutoTestDiscoveryChanged. */
	UFUNCTION(BlueprintCallable, Category = "AutoTest")
	int32 Refresh();

	/** Cached last discovery. */
	const TArray<FAutoTestInfo>& GetDiscoveredTests() const { return DiscoveredTests; }

	// --- Execution ---
	/** Run a single test by file path. Returns false if a run is already in progress. */
	bool RunTest(const FString& FilePath);

	/** Run all discovered tests. */
	UFUNCTION(BlueprintCallable, Category = "AutoTest")
	void RunAll();

	/** Run tests matching a filter: "all", "name:Foo", or "tag:smoke". */
	UFUNCTION(BlueprintCallable, Category = "AutoTest")
	void RunByFilter(const FString& Filter);

	/** True while a suite is running. */
	UFUNCTION(BlueprintPure, Category = "AutoTest")
	bool IsRunning() const { return bIsRunning; }

	/** Results of the most recent suite run. */
	const FAutoTestSuiteResult& GetLastResults() const { return LastResults; }

	// --- Delegates (editor panel binds to these) ---
	FOnAutoTestTestComplete& OnTestComplete() { return TestCompleteDelegate; }
	FOnAutoTestRunComplete& OnRunComplete() { return RunCompleteDelegate; }
	FOnAutoTestRequestOpenPanel& OnRequestOpenPanel() { return RequestOpenPanelDelegate; }
	FOnAutoTestDiscoveryChanged& OnDiscoveryChanged() { return DiscoveryChangedDelegate; }

private:
	// --- Console commands (registered in Initialize) ---
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();
	void Cmd_Run(const TArray<FString>& Args);
	void Cmd_RunAll(const TArray<FString>& Args);
	void Cmd_OpenPanel(const TArray<FString>& Args);
	void Cmd_Refresh(const TArray<FString>& Args);
	TArray<FName> ConsoleCommandNames;

	// --- Tick (FTicker) ---
	bool OnTick(float DeltaSeconds);
	FTSTicker::FDelegateHandle TickHandle;

	// --- Per-run state ---
	void StartSuite(const TArray<FAutoTestInfo>& Tests);
	bool StartNextTest();
	void FinalizeTest();
	void FinalizeSuite();
	void AbortRun(const FString& Reason);

	// RAII-style save/restore of the BehaviorU.TickRate console variable for determinism.
	void SaveAndForceTickRate();
	void RestoreTickRate();
	float SavedTickRate = -1.0f;

	TArray<FAutoTestInfo> DiscoveredTests;

	bool bIsRunning = false;
	TArray<FAutoTestInfo> PendingQueue;

	// Active test state.
	TUniquePtr<FAutoTestRunContext> CurrentContext;
	FAutoTestInfo CurrentInfo;
	TWeakObjectPtr<AActor> TestActor;
	UPROPERTY() UAutoTestAgent* TestAgent = nullptr;
	double WatchdogEndTime = 0.0;
	bool bPendingFinalize = false; // one-frame defer so the worker-thread tick flushes

	FAutoTestSuiteResult LastResults;
	double SuiteStartTime = 0.0;

	// Delegates.
	FOnAutoTestTestComplete TestCompleteDelegate;
	FOnAutoTestRunComplete RunCompleteDelegate;
	FOnAutoTestRequestOpenPanel RequestOpenPanelDelegate;
	FOnAutoTestDiscoveryChanged DiscoveryChangedDelegate;
};