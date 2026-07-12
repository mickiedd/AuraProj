// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestRunnerSubsystem.h"
#include "AutoTestAgent.h"
#include "AutoTestLog.h"
#include "AutoTestReportWriter.h"
#include "BehaviorUTypes.h"
#include "BehaviorUAgent.h"

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/FeedbackContext.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/UObjectGlobals.h"
#include "XmlFile.h"

// ===================================================================
// Helpers
// ===================================================================

static UWorld* GetActiveWorld(const UGameInstanceSubsystem* Self)
{
	UWorld* World = Self ? Self->GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	// Accept PIE and standalone/game worlds.
	const EWorldType::Type Type = World->WorldType;
	if (Type == EWorldType::PIE || Type == EWorldType::Game)
	{
		return World;
	}
	return nullptr;
}

/** Scan a directory recursively for *.xml files. */
static void ScanDirForXml(const FString& Dir, TArray<FString>& OutPaths)
{
	if (Dir.IsEmpty() || !IFileManager::Get().DirectoryExists(*Dir))
	{
		return;
	}

	TArray<FString> Found;
	IFileManager::Get().FindFilesRecursive(Found, *Dir, TEXT("*.xml"), true, false, false);
	for (const FString& F : Found)
	{
		OutPaths.AddUnique(FPaths::ConvertRelativePathToFull(F));
	}
}

/** Parse test metadata from the <behavior> root <property> children. */
static FAutoTestInfo ParseTestInfo(const FString& FilePath)
{
	FAutoTestInfo Info;
	Info.FilePath = FilePath;
	Info.Name = FPaths::GetBaseFilename(FilePath);
	Info.Timeout = 60.0f;

	FXmlFile Doc;
	if (!Doc.LoadFile(FilePath))
	{
		return Info;
	}

	const FXmlNode* Root = Doc.GetRootNode(); // <behavior ...>
	if (!Root)
	{
		return Info;
	}

	for (const FXmlNode* Child : Root->GetChildrenNodes())
	{
		if (!Child || Child->GetTag() != TEXT("property"))
		{
			continue;
		}
		const FString Name = Child->GetAttribute(TEXT("name"));
		const FString Value = Child->GetAttribute(TEXT("value"));
		if (Name == TEXT("name"))			Info.Name = Value;
		else if (Name == TEXT("description"))	Info.Description = Value;
		else if (Name == TEXT("timeout"))		Info.Timeout = FCString::Atof(*Value);
		else if (Name == TEXT("tags"))
		{
			TArray<FString> Tags;
			Value.ParseIntoArray(Tags, TEXT(","), true);
			for (FString& T : Tags) { T.TrimStartInline(); T.TrimEndInline(); }
			Info.Tags = MoveTemp(Tags);
		}
	}
	return Info;
}

static const TCHAR* GAutoTestPluginName = TEXT("AuraAutoTestPlugin");

// ===================================================================
// Lifecycle
// ===================================================================

void UAutoTestRunnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DiscoveredTests = DiscoverTests();

	RegisterConsoleCommands();

	// Persistent ticker drives per-test watchdog/completion polling on the game thread.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UAutoTestRunnerSubsystem::OnTick),
		0.0f);

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Runner initialized; %d test(s) discovered."), DiscoveredTests.Num());
}

void UAutoTestRunnerSubsystem::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	UnregisterConsoleCommands();

	if (bIsRunning)
	{
		AbortRun(TEXT("Subsystem deinitialized"));
	}

	Super::Deinitialize();
}

// ===================================================================
// Discovery
// ===================================================================

TArray<FAutoTestInfo> UAutoTestRunnerSubsystem::DiscoverTests() const
{
	TArray<FString> Paths;

	// Plugin-shipped sample tests.
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(GAutoTestPluginName))
	{
		ScanDirForXml(Plugin->GetBaseDir() / TEXT("Content/AutoTests"), Paths);
	}
	// Project-authored tests.
	ScanDirForXml(FPaths::ProjectContentDir() / TEXT("AutoTests"), Paths);

	TArray<FAutoTestInfo> Infos;
	Infos.Reserve(Paths.Num());
	for (const FString& P : Paths)
	{
		Infos.Add(ParseTestInfo(P));
	}
	return Infos;
}

int32 UAutoTestRunnerSubsystem::Refresh()
{
	DiscoveredTests = DiscoverTests();
	DiscoveryChangedDelegate.Broadcast();
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Refresh: %d test(s) discovered."), DiscoveredTests.Num());
	return DiscoveredTests.Num();
}

// ===================================================================
// Execution entry points
// ===================================================================

bool UAutoTestRunnerSubsystem::RunTest(const FString& FilePath)
{
	if (bIsRunning)
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] A run is already in progress."));
		return false;
	}

	TArray<FAutoTestInfo> Tests;
	FAutoTestInfo Info = ParseTestInfo(FilePath);
	if (!FPaths::FileExists(Info.FilePath))
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] Test file not found: %s"), *FilePath);
		return false;
	}
	Tests.Add(MoveTemp(Info));
	StartSuite(Tests);
	return true;
}

void UAutoTestRunnerSubsystem::RunAll()
{
	if (bIsRunning)
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] A run is already in progress."));
		return;
	}
	if (DiscoveredTests.Num() == 0)
	{
		Refresh();
	}
	StartSuite(DiscoveredTests);
}

void UAutoTestRunnerSubsystem::RunByFilter(const FString& Filter)
{
	if (bIsRunning)
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] A run is already in progress."));
		return;
	}
	if (DiscoveredTests.Num() == 0)
	{
		Refresh();
	}

	TArray<FAutoTestInfo> Selected;
	const FString Trimmed = Filter.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed == TEXT("all"))
	{
		Selected = DiscoveredTests;
	}
	else if (Trimmed.StartsWith(TEXT("name:")))
	{
		const FString Target = Trimmed.Mid(5);
		for (const FAutoTestInfo& Info : DiscoveredTests)
		{
			if (Info.Name == Target)
			{
				Selected.Add(Info);
			}
		}
	}
	else if (Trimmed.StartsWith(TEXT("tag:")))
	{
		const FString Target = Trimmed.Mid(4);
		for (const FAutoTestInfo& Info : DiscoveredTests)
		{
			if (Info.Tags.Contains(Target))
			{
				Selected.Add(Info);
			}
		}
	}
	else
	{
		// Treat as a name match.
		for (const FAutoTestInfo& Info : DiscoveredTests)
		{
			if (Info.Name == Trimmed)
			{
				Selected.Add(Info);
			}
		}
	}

	if (Selected.Num() == 0)
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] No tests matched filter '%s'."), *Filter);
		return;
	}
	StartSuite(Selected);
}

void UAutoTestRunnerSubsystem::StopRun()
{
	if (!bIsRunning)
	{
		UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] StopRun: no run in progress."));
		return;
	}

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Stop requested; finishing current test and skipping the rest."));
	bStopRequested = true;
	PendingQueue.Empty(); // do not start any further tests

	if (CurrentContext)
	{
		// Mark the in-progress test Aborted (no-op if it already reached a terminal
		// Pass/Fail/etc. this frame). Defer finalization one tick so the worker-thread
		// Phase 2 flushes, then FinalizeTest records the result and winds the suite down.
		CurrentContext->SetTerminal(EAutoTestStatus::Aborted);
		bPendingFinalize = true;
	}
	else
	{
		// Between tests with nothing to finalize: wind the suite down directly.
		RestoreTickRate();
		FinalizeSuite();
	}
}

// ===================================================================
// Per-run flow
// ===================================================================

void UAutoTestRunnerSubsystem::StartSuite(const TArray<FAutoTestInfo>& Tests)
{
	UWorld* World = GetActiveWorld(this);
	if (!World)
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] No active PIE/game world. Start PIE first."));
		return;
	}

	bIsRunning = true;
	bStopRequested = false;
	PendingQueue = Tests;

	LastResults = FAutoTestSuiteResult();
	LastResults.Timestamp = FDateTime::UtcNow();
	SuiteStartTime = FPlatformTime::Seconds();

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Starting suite: %d test(s)."), PendingQueue.Num());
	StartNextTest();
}

bool UAutoTestRunnerSubsystem::StartNextTest()
{
	if (PendingQueue.Num() == 0)
	{
		FinalizeSuite();
		return false;
	}

	UWorld* World = GetActiveWorld(this);
	if (!World)
	{
		AbortRun(TEXT("World lost during run"));
		return false;
	}

	CurrentInfo = PendingQueue[0];
	PendingQueue.RemoveAt(0, 1, false);

	// Fresh per-test context.
	CurrentContext = MakeUnique<FAutoTestRunContext>();
	CurrentContext->TestName = CurrentInfo.Name;
	CurrentContext->FilePath = CurrentInfo.FilePath;
	CurrentContext->StartTime.store(World->GetTimeSeconds(), std::memory_order_relaxed);

	bPendingFinalize = false;

	// Spawn a transient host actor far away so it does not pollute the level.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Name = TEXT("AutoTestHost");
	// FActorSpawnParameters defaults NameMode to Required_Fatal. A destroyed-but-not-yet-GC'd
	// host from a previous run still owns the "AutoTestHost" name, so reusing it would fatal
	// ("Cannot generate unique name..."). Requested resolves the collision to AutoTestHost_1, etc.
	SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0, 0, -1000000)), SpawnParams);
	if (!Host)
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] Failed to spawn test host actor."));
		AbortRun(TEXT("SpawnActor failed"));
		return false;
	}
	TestActor = Host;

	// Create + register the test agent. RegisterComponent triggers BeginPlay, which
	// auto-registers with UBehaviorUWorldSubsystem (no manual ticking needed).
	TestAgent = NewObject<UAutoTestAgent>(Host);
	TestAgent->RegisterComponent();
	TestAgent->SetRunContext(CurrentContext.Get());

	// Wire the run context BEFORE loading the tree so the first tick's nodes see it.
	if (!TestAgent->LoadBehaviorTreeFromXMLFile(CurrentInfo.FilePath))
	{
		UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] Failed to load test tree: %s"), *CurrentInfo.FilePath);
		// Record an error result and move on.
		FAutoTestResult Err;
		Err.Name = CurrentInfo.Name;
		Err.FilePath = CurrentInfo.FilePath;
		Err.Status = EAutoTestStatus::Error;
		Err.Error = TEXT("Failed to load behavior tree");
		LastResults.Tests.Add(MoveTemp(Err));
		TestAgent = nullptr;
		if (Host) { World->DestroyActor(Host); }
		TestActor = nullptr;
		return StartNextTest();
	}

	// Force deterministic every-frame tree ticking.
	SaveAndForceTickRate();

	// Watchdog.
	const double Now = World->GetTimeSeconds();
	WatchdogEndTime = (CurrentInfo.Timeout > 0.0f) ? Now + CurrentInfo.Timeout : Now + 60.0 * 60.0;

	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Running: %s"), *CurrentInfo.Name);
	return true;
}

void UAutoTestRunnerSubsystem::FinalizeTest()
{
	UWorld* World = GetActiveWorld(this);
	const double EndTime = World ? World->GetTimeSeconds() : FPlatformTime::Seconds();

	// Build the result from the context (safe: worker tick finished this frame).
	FAutoTestResult Result;
	Result.Name = CurrentInfo.Name;
	Result.FilePath = CurrentInfo.FilePath;
	Result.Status = CurrentContext ? CurrentContext->GetStatus() : EAutoTestStatus::Error;
	if (CurrentContext)
	{
		Result.Assertions = CurrentContext->GetAssertions();
	}
	Result.DurationMs = (EndTime - CurrentContext->StartTime.load(std::memory_order_relaxed)) * 1000.0;

	LastResults.Tests.Add(MoveTemp(Result));

	// Cleanup spawned actors first (owned by the world, recorded in the context).
	if (CurrentContext)
	{
		TArray<TWeakObjectPtr<AActor>> Spawned = CurrentContext->GetSpawnedActors();
		if (World)
		{
			for (const TWeakObjectPtr<AActor>& Weak : Spawned)
			{
				if (AActor* A = Weak.Get())
				{
					World->DestroyActor(A);
				}
			}
		}
	}

	// Destroy the host actor (EndPlay unregisters the agent from the subsystem).
	if (World && TestActor.IsValid())
	{
		World->DestroyActor(TestActor.Get());
	}
	TestActor = nullptr;
	TestAgent = nullptr;

	RestoreTickRate();

	const FAutoTestResult& Broadcast = LastResults.Tests.Last();
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] %s: %s (%.1f ms)"),
		*Broadcast.Name,
		Broadcast.Status == EAutoTestStatus::Pass ? TEXT("PASS") :
		Broadcast.Status == EAutoTestStatus::Timeout ? TEXT("TIMEOUT") :
		Broadcast.Status == EAutoTestStatus::Error ? TEXT("ERROR") : TEXT("FAIL"),
		Broadcast.DurationMs);

	TestCompleteDelegate.Broadcast(Broadcast);

	StartNextTest();
}

void UAutoTestRunnerSubsystem::FinalizeSuite()
{
	const double TotalDuration = (FPlatformTime::Seconds() - SuiteStartTime) * 1000.0;
	LastResults.DurationMs = TotalDuration;

	int32 Passed = 0, Failed = 0, TimedOut = 0, Errored = 0, Aborted = 0;
	for (const FAutoTestResult& T : LastResults.Tests)
	{
		switch (T.Status)
		{
		case EAutoTestStatus::Pass:		++Passed; break;
		case EAutoTestStatus::Fail:		++Failed; break;
		case EAutoTestStatus::Timeout:	++TimedOut; break;
		case EAutoTestStatus::Error:		++Errored; break;
		case EAutoTestStatus::Aborted:	++Aborted; break;
		default: break;
		}
	}
	LastResults.Total = LastResults.Tests.Num();
	LastResults.Passed = Passed;
	LastResults.Failed = Failed;
	LastResults.TimedOut = TimedOut;
	LastResults.Errored = Errored;
	LastResults.Aborted = Aborted;

	// Write the JSON report.
	LastResults.ReportPath = WriteAutoTestReport(LastResults);

	UE_LOG(LogAuraTest, Log,
		TEXT("[AutoTest] Suite complete: %d total, %d passed, %d failed, %d timeout, %d error, %d aborted (%.1f ms). Report: %s"),
		LastResults.Total, LastResults.Passed, LastResults.Failed, LastResults.TimedOut, LastResults.Errored,
		LastResults.Aborted, LastResults.DurationMs, *LastResults.ReportPath);

	bIsRunning = false;
	RunCompleteDelegate.Broadcast(LastResults);
}

void UAutoTestRunnerSubsystem::AbortRun(const FString& Reason)
{
	UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] Run aborted: %s"), *Reason);

	RestoreTickRate();

	if (UWorld* World = GetActiveWorld(this))
	{
		if (CurrentContext)
		{
			for (const TWeakObjectPtr<AActor>& Weak : CurrentContext->GetSpawnedActors())
			{
				if (AActor* A = Weak.Get()) { World->DestroyActor(A); }
			}
		}
		if (TestActor.IsValid()) { World->DestroyActor(TestActor.Get()); }
	}
	TestActor = nullptr;
	TestAgent = nullptr;
	CurrentContext.Reset();
	PendingQueue.Empty();
	bPendingFinalize = false;
	bStopRequested = false;
	bIsRunning = false;
}

// ===================================================================
// Tick
// ===================================================================

bool UAutoTestRunnerSubsystem::OnTick(float DeltaSeconds)
{
	if (!bIsRunning || !CurrentContext)
	{
		return true;
	}

	// One-frame defer: when the test becomes terminal, wait one more tick so the
	// worker-thread Phase 2 has fully flushed before we read results.
	if (bPendingFinalize)
	{
		FinalizeTest();
		return true;
	}

	if (CurrentContext->IsTerminal())
	{
		bPendingFinalize = true;
		return true;
	}

	// Watchdog.
	if (UWorld* World = GetActiveWorld(this))
	{
		if (World->GetTimeSeconds() >= WatchdogEndTime)
		{
			UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] Watchdog timeout for %s"), *CurrentInfo.Name);
			CurrentContext->SetTerminal(EAutoTestStatus::Timeout);
			bPendingFinalize = true;
		}
	}
	return true;
}

// ===================================================================
// TickRate save/restore
// ===================================================================

void UAutoTestRunnerSubsystem::SaveAndForceTickRate()
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("BehaviorU.TickRate")))
	{
		SavedTickRate = CVar->GetFloat();
		CVar->Set(0.0f, ECVF_SetByConsole); // 0 = tick every frame
	}
	else
	{
		SavedTickRate = -1.0f;
	}
}

void UAutoTestRunnerSubsystem::RestoreTickRate()
{
	if (SavedTickRate >= 0.0f)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("BehaviorU.TickRate")))
		{
			CVar->Set(SavedTickRate, ECVF_SetByConsole);
		}
		SavedTickRate = -1.0f;
	}
}

// ===================================================================
// Console commands
// ===================================================================

void UAutoTestRunnerSubsystem::RegisterConsoleCommands()
{
	// UAutoTestRunnerSubsystem is a GameInstanceSubsystem, so both the editor's
	// GameInstance and the PIE GameInstance instantiate one. Console commands are
	// process-global, so register them exactly once (idempotent via FindConsoleObject)
	// and leave them registered for the process lifetime — unregistering from one
	// instance would yank commands the other instance still uses.
	IConsoleManager& CM = IConsoleManager::Get();
	if (CM.FindConsoleObject(TEXT("AutoTest.Run")) != nullptr)
	{
		return; // already registered by another instance
	}

	// ECVF_Cheat: the Run/RunAll commands spawn actors and drive behavior-tree scripts
	// against the live world, so they are a cheat surface in shipping builds. The OpenPanel
	// and Refresh commands are harmless (editor-only UI / file scan) and stay ECVF_Default.
	auto Register = [&](const TCHAR* Name, const TCHAR* Help, void (UAutoTestRunnerSubsystem::*Fn)(const TArray<FString>&), uint64 Flags)
	{
		CM.RegisterConsoleCommand(Name, Help, FConsoleCommandWithArgsDelegate::CreateUObject(this, Fn), Flags);
		ConsoleCommandNames.Add(FName(Name));
	};

	Register(TEXT("AutoTest.Run"),       TEXT("Run a test. Usage: AutoTest.Run [all|name:Foo|tag:smoke|<name>]"), &UAutoTestRunnerSubsystem::Cmd_Run,      ECVF_Cheat);
	Register(TEXT("AutoTest.RunAll"),     TEXT("Run all discovered tests."),                                                &UAutoTestRunnerSubsystem::Cmd_RunAll,   ECVF_Cheat);
	Register(TEXT("AutoTest.Stop"),       TEXT("Stop the in-progress AutoTest suite (current test is marked Aborted)."),    &UAutoTestRunnerSubsystem::Cmd_Stop,     ECVF_Cheat);
	Register(TEXT("AutoTest.OpenPanel"),  TEXT("Open the AutoTest results panel (editor)."),                                &UAutoTestRunnerSubsystem::Cmd_OpenPanel, ECVF_Default);
	Register(TEXT("AutoTest.Refresh"),    TEXT("Re-scan test directories."),                                                &UAutoTestRunnerSubsystem::Cmd_Refresh,  ECVF_Default);
}

void UAutoTestRunnerSubsystem::UnregisterConsoleCommands()
{
	// Intentionally a no-op: commands are process-global and shared across subsystem
	// instances (see RegisterConsoleCommands). They live for the process lifetime.
}

void UAutoTestRunnerSubsystem::Cmd_Run(const TArray<FString>& Args)
{
	if (Args.Num() == 0)
	{
		RunAll();
	}
	else
	{
		RunByFilter(Args[0]);
	}
}

void UAutoTestRunnerSubsystem::Cmd_RunAll(const TArray<FString>& Args)
{
	RunAll();
}

void UAutoTestRunnerSubsystem::Cmd_Stop(const TArray<FString>& Args)
{
	StopRun();
}

void UAutoTestRunnerSubsystem::Cmd_OpenPanel(const TArray<FString>& Args)
{
	RequestOpenPanelDelegate.Broadcast();
}

void UAutoTestRunnerSubsystem::Cmd_Refresh(const TArray<FString>& Args)
{
	Refresh();
}