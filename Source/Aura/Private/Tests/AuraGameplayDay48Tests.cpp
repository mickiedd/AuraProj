// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraObjectiveTypes.h"
#include "AuraGameplayDay47Harness.h"

namespace AuraGameplayDay48TestsPrivate
{
	FGuid RunId()
	{
		return FGuid(48, 2026, 9, 4);
	}

	FAuraObjectiveDefinition MakeDefinition(FName Id, EAuraObjectiveType Type, FName TargetId, int32 TargetCount,
		bool bRequired = true)
	{
		FAuraObjectiveDefinition Definition;
		Definition.Id = Id;
		Definition.Type = Type;
		Definition.TargetId = TargetId;
		Definition.TargetCount = TargetCount;
		Definition.bRequired = bRequired;
		Definition.DeadlineSeconds = 180.0;
		return Definition;
	}

	FAuraObjectiveRunState MakeClearRun(FString& OutError)
	{
		FAuraObjectiveRunState State;
		State.Initialize(RunId(), 1);
		FAuraObjectiveDefinition Clear = MakeDefinition(TEXT("assault_clear"), EAuraObjectiveType::Clear,
			TEXT("assault_cell_1"), 2);
		TArray<FAuraObjectiveDefinition> Definitions;
		Definitions.Add(Clear);
		State.ConfigureObjectives(Definitions, OutError);
		State.TryActivateNext(10.0, OutError);
		return State;
	}

	FAuraObjectiveRunState MakeRelayRun(FString& OutError)
	{
		FAuraObjectiveRunState State;
		State.Initialize(RunId(), 1);
		TArray<FAuraObjectiveDefinition> Definitions;
		Definitions.Add(MakeDefinition(TEXT("relay"), EAuraObjectiveType::InteractHold, TEXT("relay_1"), 1));
		State.ConfigureObjectives(Definitions, OutError);
		State.TryActivateNext(10.0, OutError);
		return State;
	}

	FAuraObjectiveRunState MakeEscortRun(FString& OutError)
	{
		FAuraObjectiveRunState State;
		State.Initialize(RunId(), 1);
		FAuraObjectiveDefinition Escort = MakeDefinition(TEXT("escort"), EAuraObjectiveType::Escort,
			TEXT("shelter_a"), 2);
		Escort.RequiredMemberIds = {TEXT("civilian_a"), TEXT("civilian_b")};
		TArray<FAuraObjectiveDefinition> Definitions;
		Definitions.Add(Escort);
		State.ConfigureObjectives(Definitions, OutError);
		State.TryActivateNext(10.0, OutError);
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48ForeignOutcomeIgnored,
	"Aura.Gameplay.Day48.ForeignOutcomeIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48ForeignOutcomeIgnored::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraObjectiveRunState State = AuraGameplayDay48TestsPrivate::MakeClearRun(Error);
	TestEqual(TEXT("A foreign run death cannot advance the active objective"), State.RegisterClearDeath(
		FGuid(99, 2, 3, 4), 1, TEXT("assault_clear"), 1, 1, Error), EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("Foreign event leaves progress at zero"), State.BuildPublicSnapshot().CurrentProgressCount, 0);
	TestEqual(TEXT("The local run accepts its own death"), State.RegisterClearDeath(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("assault_clear"), 1, 1, Error), EAuraObjectiveMutationResult::Accepted);

	FAuraObjectiveRunState EscortState = AuraGameplayDay48TestsPrivate::MakeEscortRun(Error);
	TestEqual(TEXT("A foreign escort arrival cannot advance rescue"), EscortState.RegisterEscortArrival(
		FGuid(99, 2, 3, 4), 1, TEXT("escort"), TEXT("civilian_a"), TEXT("shelter_a"), 1, Error),
		EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("A stale epoch escort arrival cannot advance rescue"), EscortState.RegisterEscortArrival(
		AuraGameplayDay48TestsPrivate::RunId(), 2, TEXT("escort"), TEXT("civilian_a"), TEXT("shelter_a"), 1, Error),
		EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("Foreign rescue events leave progress at zero"),
		EscortState.BuildPublicSnapshot().CurrentProgressCount, 0);

	FAuraObjectiveRunState RelayState = AuraGameplayDay48TestsPrivate::MakeRelayRun(Error);
	TestEqual(TEXT("A foreign channel claim cannot acquire the relay"), RelayState.TryBeginInteractHold(
		FGuid(99, 2, 3, 4), 1, TEXT("relay"), TEXT("player_foreign"), 1, 10.0, true, 100.0f, true,
		Error), EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("The authoritative owner can still acquire the unmodified relay"), RelayState.TryBeginInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_local"), 1, 10.0, true, 100.0f,
		true, Error), EAuraObjectiveMutationResult::Accepted);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48ObjectiveDAGRejected,
	"Aura.Gameplay.Day48.ObjectiveDAGRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48ObjectiveDAGRejected::RunTest(const FString& Parameters)
{
	FAuraObjectiveRunState State;
	TestTrue(TEXT("Objective state initializes"), State.Initialize(AuraGameplayDay48TestsPrivate::RunId(), 1));
	FAuraObjectiveDefinition First = AuraGameplayDay48TestsPrivate::MakeDefinition(TEXT("first"),
		EAuraObjectiveType::Clear, TEXT("cell_1"), 1);
	FAuraObjectiveDefinition Second = AuraGameplayDay48TestsPrivate::MakeDefinition(TEXT("second"),
		EAuraObjectiveType::Clear, TEXT("cell_2"), 1);
	First.PrerequisiteIds.Add(Second.Id);
	Second.PrerequisiteIds.Add(First.Id);
	FString Error;
	TestFalse(TEXT("A cyclic objective graph is rejected"), State.ConfigureObjectives({First, Second}, Error));
	Second.PrerequisiteIds.Reset();
	First.PrerequisiteIds = {TEXT("missing")};
	TestFalse(TEXT("A missing objective successor is rejected"), State.ConfigureObjectives({First, Second}, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48ChannelRaceSingleOwner,
	"Aura.Gameplay.Day48.ChannelRaceSingleOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48ChannelRaceSingleOwner::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraObjectiveRunState State = AuraGameplayDay48TestsPrivate::MakeRelayRun(Error);
	TestEqual(TEXT("The first relay contender acquires the channel"), State.TryBeginInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_a"), 7, 10.0, true, 100.0f, true,
		Error), EAuraObjectiveMutationResult::Accepted);
	TestEqual(TEXT("The second contender sees the relay as busy"), State.TryBeginInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_b"), 7, 10.1, true, 100.0f, true,
		Error), EAuraObjectiveMutationResult::Busy);
	TestEqual(TEXT("Only the original owner can complete the relay"), State.TryCompleteInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_a"), 7, 12.9, true, 100.0f, true,
		Error), EAuraObjectiveMutationResult::Completed);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48ChannelCancelNoProgress,
	"Aura.Gameplay.Day48.ChannelCancelNoProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48ChannelCancelNoProgress::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraObjectiveRunState State = AuraGameplayDay48TestsPrivate::MakeRelayRun(Error);
	TestEqual(TEXT("Relay starts for the first owner"), State.TryBeginInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_a"), 1, 10.0, true, 100.0f, true,
		Error), EAuraObjectiveMutationResult::Accepted);
	TestEqual(TEXT("Damage/evade cancellation closes the channel without progress"), State.CancelInteractHold(TEXT("relay"),
		TEXT("player_a"), TEXT("OwnerDamage"), Error), EAuraObjectiveMutationResult::Cancelled);
	TestEqual(TEXT("Cancellation leaves the relay active at zero progress"), State.BuildPublicSnapshot().CurrentProgressCount, 0);
	TestEqual(TEXT("A new owner can retry after cancellation"), State.TryBeginInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_b"), 2, 20.0, true, 100.0f, true,
		Error), EAuraObjectiveMutationResult::Accepted);
	TestEqual(TEXT("A blocked completion cancels without progress"), State.TryCompleteInteractHold(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("relay"), TEXT("player_b"), 2, 20.5, true, 100.0f, false,
		Error), EAuraObjectiveMutationResult::Cancelled);
	TestEqual(TEXT("The blocked completion remains incomplete"), State.BuildPublicSnapshot().CurrentProgressCount, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48EscortIdentityRequired,
	"Aura.Gameplay.Day48.EscortIdentityRequired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48EscortIdentityRequired::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraObjectiveRunState State = AuraGameplayDay48TestsPrivate::MakeEscortRun(Error);
	TestEqual(TEXT("An unrelated civilian cannot complete the escort"), State.RegisterEscortArrival(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("escort"), TEXT("civilian_other"), TEXT("shelter_a"), 1,
		Error), EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("A designated civilian at the wrong destination is rejected"), State.RegisterEscortArrival(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("escort"), TEXT("civilian_a"), TEXT("shelter_b"), 1,
		Error), EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("The first designated civilian arrives once"), State.RegisterEscortArrival(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("escort"), TEXT("civilian_a"), TEXT("shelter_a"), 1,
		Error), EAuraObjectiveMutationResult::Accepted);
	TestEqual(TEXT("The duplicate arrival is rejected"), State.RegisterEscortArrival(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("escort"), TEXT("civilian_a"), TEXT("shelter_a"), 1,
		Error), EAuraObjectiveMutationResult::Duplicate);
	TestEqual(TEXT("Both designated arrivals complete the escort"), State.RegisterEscortArrival(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("escort"), TEXT("civilian_b"), TEXT("shelter_a"), 1,
		Error), EAuraObjectiveMutationResult::Completed);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48ExtractionRequiresAllPrimary,
	"Aura.Gameplay.Day48.ExtractionRequiresAllPrimary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48ExtractionRequiresAllPrimary::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraObjectiveRunState State;
	State.Initialize(AuraGameplayDay48TestsPrivate::RunId(), 1);
	TArray<FAuraObjectiveDefinition> Definitions;
	Definitions.Add(AuraGameplayDay48TestsPrivate::MakeDefinition(TEXT("required_clear"), EAuraObjectiveType::Clear,
		TEXT("cell_1"), 1, true));
	Definitions.Add(AuraGameplayDay48TestsPrivate::MakeDefinition(TEXT("optional_relay"), EAuraObjectiveType::InteractHold,
		TEXT("relay_1"), 1, false));
	TestTrue(TEXT("Required and optional objectives configure"), State.ConfigureObjectives(Definitions, Error));
	TestTrue(TEXT("The required objective activates first"), State.TryActivateNext(10.0, Error));
	TestFalse(TEXT("Extraction cannot open before the required objective completes"), State.TryBeginExtraction(Error));
	TestEqual(TEXT("Required clear completes"), State.RegisterClearDeath(AuraGameplayDay48TestsPrivate::RunId(), 1,
		TEXT("required_clear"), 1, 1, Error), EAuraObjectiveMutationResult::Completed);
	TestTrue(TEXT("Extraction opens after all required objectives complete"), State.TryBeginExtraction(Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay48GeneralizedSequenceKeepsChoices,
	"Aura.Gameplay.Day48.GeneralizedSequenceKeepsChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay48GeneralizedSequenceKeepsChoices::RunTest(const FString& Parameters)
{
	FString Error;
	FAuraObjectiveRunState Objectives;
	TestTrue(TEXT("Generalized objective state initializes"),
		Objectives.Initialize(AuraGameplayDay48TestsPrivate::RunId(), 1));
	FAuraObjectiveDefinition First = AuraGameplayDay48TestsPrivate::MakeDefinition(TEXT("assault_cell_1"),
		EAuraObjectiveType::Clear, TEXT("assault_cell_1"), 3);
	FAuraObjectiveDefinition Second = AuraGameplayDay48TestsPrivate::MakeDefinition(TEXT("assault_cell_2"),
		EAuraObjectiveType::Clear, TEXT("assault_cell_2"), 3);
	Second.PrerequisiteIds.Add(First.Id);
	TestTrue(TEXT("Two-cell generalized sequence configures"), Objectives.ConfigureObjectives({First, Second}, Error));

	FAuraDay47ContractHarness Offers;
	TestTrue(TEXT("Existing Day 47 offer owner initializes for the same run"), Offers.Initialize(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("player_a"), TEXT("Aura"), 41001, Error));
	const TArray<FName> ExpectedCells = {TEXT("assault_cell_1"), TEXT("assault_cell_2")};
	for (int32 CellIndex = 0; CellIndex < ExpectedCells.Num(); ++CellIndex)
	{
		const double CompletionTime = 100.0 + CellIndex * 100.0;
		TestTrue(TEXT("The next generalized objective activates"), Objectives.TryActivateNext(CompletionTime - 10.0, Error));
		TestEqual(TEXT("Generalized sequence preserves current-cell identity"),
			Objectives.BuildPublicSnapshot().CurrentObjectiveId, ExpectedCells[CellIndex]);
		for (int32 DeathSequence = 1; DeathSequence <= 3; ++DeathSequence)
		{
			const EAuraObjectiveMutationResult Result = Objectives.RegisterClearDeath(
				AuraGameplayDay48TestsPrivate::RunId(), 1, ExpectedCells[CellIndex], 1, DeathSequence, Error);
			TestEqual(TEXT("Generalized clear progress has one terminal result"), Result,
				DeathSequence == 3 ? EAuraObjectiveMutationResult::Completed : EAuraObjectiveMutationResult::Accepted);
		}

		TestTrue(TEXT("The matching legacy boundary still grants one choice window"),
			Offers.CompleteNextCell(CompletionTime, Error));
		const FAuraDay47OfferSnapshot Snapshot = Offers.BuildOwnerSnapshot(TEXT("player_a"));
		TestTrue(TEXT("Choice window remains owner-visible"), Snapshot.bOwnerAuthorized && Snapshot.bChoiceOpen);
		TestEqual(TEXT("Each boundary still offers exactly three choices"), Snapshot.Offers.Num(), 3);
		TestEqual(TEXT("Replaying the same completion boundary cannot mint another offer"),
			Offers.ConsumeCellCompletedBoundary(ExpectedCells[CellIndex], 1, 3, CompletionTime + 0.1, Error),
			EAuraDay47OfferResult::DuplicateBoundary);
		TestEqual(TEXT("Duplicate completion preserves the granted-boundary count"),
			Offers.GetGrantedBoundaryCount(), CellIndex + 1);
		const FName RequestId(*FString::Printf(TEXT("day48_choice_%d"), CellIndex + 1));
		TestEqual(TEXT("A visible choice remains selectable"), Offers.Choose(RequestId, Snapshot.OfferRevision,
			Snapshot.Offers[0], CompletionTime + 1.0, Error), EAuraDay47OfferResult::Selected);
	}

	TestEqual(TEXT("The generalized two-cell sequence grants exactly two offers"), Offers.GetGrantedBoundaryCount(), 2);
	TestEqual(TEXT("Both boundary choices remain selected"),
		Offers.BuildOwnerSnapshot(TEXT("player_a")).SelectedAugments.Num(), 2);
	TestEqual(TEXT("A post-terminal objective replay cannot complete again"), Objectives.RegisterClearDeath(
		AuraGameplayDay48TestsPrivate::RunId(), 1, TEXT("assault_cell_2"), 1, 3, Error),
		EAuraObjectiveMutationResult::Rejected);
	TestEqual(TEXT("Rejected objective replay cannot change offer count"), Offers.GetGrantedBoundaryCount(), 2);
	return !HasAnyErrors();
}

#endif
