// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraEncounterCoordinator.h"
#include "Gameplay/AuraMissionTypes.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace AuraGameplayDay42_43TestsPrivate
{
	bool ReadProjectFile(const TCHAR* RelativePath, FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *(FPaths::ProjectDir() / RelativePath));
	}

	FAuraMissionRunState PreparedRun()
	{
		FAuraMissionRunState State;
		FString Error;
		State.TryBeginPreparation(FGuid(1, 2, 3, 4), 1, TEXT("GameplayExpansionV1"), TEXT("Assault"), TEXT("arrangement_a"), 10.0, 1200.0, Error);
		TArray<FAuraMissionCellState> Cells;
		FAuraMissionCellState& First = Cells.AddDefaulted_GetRef();
		First.CellId = TEXT("assault_cell_1");
		First.RequiredDeaths = 2;
		FAuraMissionCellState& Second = Cells.AddDefaulted_GetRef();
		Second.CellId = TEXT("assault_cell_2");
		Second.RequiredDeaths = 1;
		State.ConfigureClearCells(Cells, Error);
		State.TryActivate(11.0, Error);
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay42RunTransitionExactlyOnce,
	"Aura.Gameplay.Day42.RunTransitionExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay42RunTransitionExactlyOnce::RunTest(const FString& Parameters)
{
	FAuraMissionRunState State = AuraGameplayDay42_43TestsPrivate::PreparedRun();
	FString Error;
	TestEqual(TEXT("Run starts active after valid preparation"), State.Phase, EAuraMissionPhase::Active);
	TestEqual(TEXT("First cell death is accepted without a boundary"),
		State.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_1"), 1, 1, Error), EAuraMissionMutationResult::Accepted);
	TestEqual(TEXT("Second cell-one death emits one cell boundary"),
		State.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_2"), 1, 1, Error), EAuraMissionMutationResult::CellCompleted);
	TestEqual(TEXT("Cell two becomes current in order"), State.GetCurrentCellId(), FName(TEXT("assault_cell_2")));
	TestEqual(TEXT("Cell-two death emits the terminal objective boundary"),
		State.RegisterCellDeath(TEXT("assault_cell_2"), TEXT("slot_3"), 1, 1, Error), EAuraMissionMutationResult::ObjectiveCompleted);
	TestTrue(TEXT("Extraction begins only after both cells"), State.TryBeginExtraction(20.0, 30.0, Error));
	TestTrue(TEXT("Settlement opens once"), State.TryBeginSettlement(true, Error));
	TestTrue(TEXT("Settlement commits once"), State.TryCommitSettlement(Error));
	const int32 TerminalRevision = State.Revision;
	TestEqual(TEXT("Run reaches one completed terminal state"), State.Phase, EAuraMissionPhase::Completed);
	TestFalse(TEXT("Duplicate terminal commit is rejected"), State.TryCommitSettlement(Error));
	TestFalse(TEXT("Failure cannot overwrite completed run"), State.TryFail(TEXT("late_failure"), Error));
	TestEqual(TEXT("Terminal revision remains stable after rejected callbacks"), State.Revision, TerminalRevision);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay42DuplicateDeathRejected,
	"Aura.Gameplay.Day42.DuplicateDeathRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay42DuplicateDeathRejected::RunTest(const FString& Parameters)
{
	FAuraMissionRunState State = AuraGameplayDay42_43TestsPrivate::PreparedRun();
	FString Error;
	TestTrue(TEXT("Initial death accepted"), State.TryRegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_1"), 1, 7, Error));
	const int32 Revision = State.Revision;
	TestFalse(TEXT("Repeated death sequence is rejected"), State.TryRegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_1"), 1, 7, Error));
	TestEqual(TEXT("Duplicate death does not advance revision"), State.Revision, Revision);
	TestFalse(TEXT("Future cell event cannot advance current cell"), State.TryRegisterCellDeath(TEXT("assault_cell_2"), TEXT("slot_9"), 1, 1, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay43StaleCellGenerationRejected,
	"Aura.Gameplay.Day43.StaleCellGenerationRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay43StaleCellGenerationRejected::RunTest(const FString& Parameters)
{
	FAuraMissionRunState State = AuraGameplayDay42_43TestsPrivate::PreparedRun();
	FString Error;
	TestFalse(TEXT("An older encounter generation cannot advance the active cell"),
		State.TryRegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_old"), 0, 1, Error));
	TestTrue(TEXT("A newer encounter generation can become the active cell generation"),
		State.TryAdvanceCurrentCellGeneration(2, Error));
	TestFalse(TEXT("The replaced generation is rejected after the advance"),
		State.TryRegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_old"), 1, 1, Error));
	TestTrue(TEXT("The current generation remains admissible"),
		State.TryRegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_new"), 2, 1, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay43CellBoundaryExactlyOnce,
	"Aura.Gameplay.Day43.CellBoundaryExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay43CellBoundaryExactlyOnce::RunTest(const FString& Parameters)
{
	FAuraMissionRunState State = AuraGameplayDay42_43TestsPrivate::PreparedRun();
	FString Error;
	TestEqual(TEXT("First cell produces one boundary receipt"),
		State.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_1"), 1, 1, Error), EAuraMissionMutationResult::Accepted);
	TestEqual(TEXT("Second cell-one death produces exactly one boundary receipt"),
		State.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_2"), 1, 1, Error), EAuraMissionMutationResult::CellCompleted);
	const int32 RevisionAfterBoundary = State.Revision;
	TestEqual(TEXT("Late duplicate cannot emit a second boundary"),
		State.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_2"), 1, 1, Error), EAuraMissionMutationResult::Rejected);
	TestEqual(TEXT("Late duplicate leaves the state revision unchanged"), State.Revision, RevisionAfterBoundary);
	TestEqual(TEXT("The next cell is the only admissible progression target"), State.GetCurrentCellId(), FName(TEXT("assault_cell_2")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay42PreparationDeadline,
	"Aura.Gameplay.Day42.PreparationDeadline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay42PreparationDeadline::RunTest(const FString& Parameters)
{
	FAuraMissionRunState State;
	FString Error;
	TestTrue(TEXT("Preparation opens"), State.TryBeginPreparation(FGuid(5, 6, 7, 8), 1, TEXT("GameplayExpansionV1"),
		TEXT("Assault"), TEXT("arrangement_a"), 10.0, 1200.0, Error));
	TArray<FAuraMissionCellState> Cells;
	FAuraMissionCellState& Cell = Cells.AddDefaulted_GetRef();
	Cell.CellId = TEXT("assault_cell_1");
	Cell.RequiredDeaths = 1;
	TestTrue(TEXT("Preparation configures its cell"), State.ConfigureClearCells(Cells, Error));
	TestFalse(TEXT("Activation at the 30-second boundary is rejected"), State.TryActivate(40.0, Error));
	TestTrue(TEXT("Preparation cancellation is allowed at the boundary"), State.TryCancelPreparationAt(40.0, TEXT("not_ready"), Error));
	TestEqual(TEXT("Cancelled preparation returns to hub"), State.Phase, EAuraMissionPhase::Hub);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay43LeaseOwnership,
	"Aura.Gameplay.Day43.LeaseOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay43LeaseOwnership::RunTest(const FString& Parameters)
{
	UAuraEncounterCoordinator* Coordinator = NewObject<UAuraEncounterCoordinator>();
	TestNotNull(TEXT("Encounter coordinator created"), Coordinator);
	if (!Coordinator) return false;
	TestTrue(TEXT("Coordinator accepts valid run identity"), Coordinator->Initialize(FGuid(1, 2, 3, 4), 1, 1));
	FString Error;
	TestTrue(TEXT("Pending slot reserves within capacity"), Coordinator->TryReserveSlot(TEXT("cell_1"), TEXT("slot_1"), 1, TEXT("MissionEncounterDriver"), 1, Error));
	TestFalse(TEXT("Second pending slot cannot exceed capacity"), Coordinator->TryReserveSlot(TEXT("cell_1"), TEXT("slot_2"), 1, TEXT("MissionEncounterDriver"), 1, Error));
	TestTrue(TEXT("Valid lease commits for its sole driver"), Coordinator->CommitSlot(TEXT("slot_1"), 1, TEXT("MissionEncounterDriver"), 1, Error));
	TestFalse(TEXT("A different driver cannot take over the live slot"), Coordinator->CommitSlot(TEXT("slot_1"), 1, TEXT("LegacyBehaviorDriver"), 1, Error));
	TestTrue(TEXT("The reserved driver is the sole owner"), Coordinator->IsDriverOwner(TEXT("slot_1"), 1, TEXT("MissionEncounterDriver"), 1));
	TestFalse(TEXT("A replacement source life cannot report the old slot death"),
		Coordinator->RecordAcceptedDeath(TEXT("cell_1"), TEXT("slot_1"), 1, 2, 1, Error));
	TestTrue(TEXT("Accepted death is recorded once"), Coordinator->RecordAcceptedDeath(TEXT("cell_1"), TEXT("slot_1"), 1, 1, 1, Error));
	TestFalse(TEXT("Duplicate accepted death is rejected"), Coordinator->RecordAcceptedDeath(TEXT("cell_1"), TEXT("slot_1"), 1, 1, 2, Error));
	TestFalse(TEXT("Stale generation cannot release live lease"), Coordinator->ReleaseSlot(TEXT("slot_1"), 2, TEXT("MissionEncounterDriver"), 1, Error));
	TestTrue(TEXT("Current generation releases terminal lease"), Coordinator->ReleaseSlot(TEXT("slot_1"), 1, TEXT("MissionEncounterDriver"), 1, Error));
	TestEqual(TEXT("Lease ledger is empty after release"), Coordinator->GetLiveCount(), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDefinitionFilesContract,
	"Aura.Gameplay.Day42.DefinitionFilesContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDefinitionFilesContract::RunTest(const FString& Parameters)
{
	for (const TCHAR* Path : {
		TEXT("Content/Config/GameplayMissionDefinitions.json"),
		TEXT("Content/Config/GameplayRoleLoadouts.json"),
		TEXT("Content/Config/GameplayEncounterDefinitions.json"),
		TEXT("Content/Config/GameplayEnemyArchetypes.json"),
		TEXT("Content/Config/GameplayArenaLayouts.json")})
	{
		TestTrue(FString::Printf(TEXT("Definition exists: %s"), Path), IFileManager::Get().FileExists(*(FPaths::ProjectDir() / Path)));
	}
	FString Layout;
	if (AuraGameplayDay42_43TestsPrivate::ReadProjectFile(TEXT("Content/Config/GameplayArenaLayouts.json"), Layout))
	{
		TestTrue(TEXT("Layout remains explicitly unverified until the engine survey exists"), Layout.Contains(TEXT("\"verified\": false")));
		TestTrue(TEXT("Layout records the typed survey blocker"), Layout.Contains(TEXT("ANCHOR_SURVEY_UNVERIFIED")));
	}
	return !HasAnyErrors();
}

#endif
