// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraEncounterCoordinator.h"
#include "Gameplay/AuraEncounterPacingTypes.h"

namespace AuraGameplayDay50TestsPrivate
{
	FGuid RunId(int32 Suffix = 50)
	{
		return FGuid(50, 2026, 9, Suffix);
	}

	FAuraPacingParticipantState Participant(FName Id = TEXT("player_a"), bool bConnected = true,
		bool bAlive = true, bool bProxy = false, float Health = 1.0f)
	{
		FAuraPacingParticipantState Result;
		Result.ParticipantId = Id;
		Result.bConnected = bConnected;
		Result.bAlive = bAlive;
		Result.bTargetableDormantProxy = bProxy;
		Result.HealthNormalized = Health;
		return Result;
	}

	TArray<FAuraPacingParticipantState> ConnectedParticipants(int32 Count = 1)
	{
		TArray<FAuraPacingParticipantState> Result;
		Result.Add(Participant(TEXT("player_a")));
		if (Count == 2) Result.Add(Participant(TEXT("player_b")));
		return Result;
	}

	TArray<FAuraPacingRosterSlot> RaiderRoster(int32 Count)
	{
		TArray<FAuraPacingRosterSlot> Result;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FAuraPacingRosterSlot& Slot = Result.AddDefaulted_GetRef();
			Slot.SlotId = FName(*FString::Printf(TEXT("raider_%02d"), Index + 1));
			Slot.ArchetypeId = TEXT("Raider");
			Slot.Cost = FAuraEncounterPacingState::CostForArchetype(Slot.ArchetypeId);
		}
		return Result;
	}

	TArray<FAuraPacingRosterSlot> MixedRoster()
	{
		const TArray<FName> Archetypes = {
			TEXT("Raider"), TEXT("Lancer"), TEXT("Bulwark"), TEXT("Disruptor"),
			TEXT("Raider"), TEXT("Lancer"), TEXT("Raider"), TEXT("Raider")
		};
		TArray<FAuraPacingRosterSlot> Result;
		for (int32 Index = 0; Index < Archetypes.Num(); ++Index)
		{
			FAuraPacingRosterSlot& Slot = Result.AddDefaulted_GetRef();
			Slot.SlotId = FName(*FString::Printf(TEXT("mixed_%02d"), Index + 1));
			Slot.ArchetypeId = Archetypes[Index];
			Slot.Cost = FAuraEncounterPacingState::CostForArchetype(Slot.ArchetypeId);
		}
		return Result;
	}

	bool InitializeWithRoster(FAuraEncounterPacingState& State,
		const TArray<FAuraPacingRosterSlot>& Roster, int32 ParticipantCount = 1,
		int32 Seed = 50001, int32 Suffix = 50)
	{
		FString Error;
		return State.Initialize(true, RunId(Suffix), 1, ParticipantCount, Seed, 0.0, 300.0, Roster, Error);
	}

	bool Initialize(FAuraEncounterPacingState& State, int32 RosterCount, int32 ParticipantCount = 1,
		int32 Seed = 50001, int32 Suffix = 50)
	{
		return InitializeWithRoster(State, RaiderRoster(RosterCount), ParticipantCount, Seed, Suffix);
	}

	EAuraPacingMutationResult EnterPressure(FAuraEncounterPacingState& State, int32 ParticipantCount = 1)
	{
		FString Error;
		return State.Tick(true, 10.0, 0.2, 0.2, ConnectedParticipants(ParticipantCount), false, Error);
	}

	int32 AdmitUntilDeferred(FAuraEncounterPacingState& State, double AuthorityNow,
		TArray<FName>* OutAdmitted = nullptr)
	{
		int32 Count = 0;
		FString Error;
		for (;;)
		{
			FName SlotId;
			const EAuraPacingMutationResult Result = State.TryAdmitNext(true, AuthorityNow, SlotId, Error);
			if (Result != EAuraPacingMutationResult::AdmissionGranted) break;
			++Count;
			if (OutAdmitted) OutAdmitted->Add(SlotId);
		}
		return Count;
	}

	bool MarkTerminal(FAuraEncounterPacingState& State, const TArray<FName>& SlotIds)
	{
		FString Error;
		for (const FName SlotId : SlotIds)
		{
			const EAuraPacingMutationResult Result = State.MarkSlotTerminal(true, SlotId, 1, 1, Error);
			if (Result != EAuraPacingMutationResult::SlotTerminal
				&& Result != EAuraPacingMutationResult::RosterComplete) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50HysteresisPreventsChatter,
	"Aura.Gameplay.Day50.HysteresisPreventsChatter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50HysteresisPreventsChatter::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState State;
	FString Error;
	TestTrue(TEXT("Pacing state initializes"), AuraGameplayDay50TestsPrivate::Initialize(State, 3));
	TestEqual(TEXT("Minimum build duration opens pressure"), AuraGameplayDay50TestsPrivate::EnterPressure(State),
		EAuraPacingMutationResult::PhaseChanged);
	for (int32 Step = 1; Step <= 20; ++Step)
	{
		const double Signal = Step % 2 == 0 ? 0.76 : 0.74;
		State.Tick(true, 10.0 + Step * 0.5, Signal, 0.0,
			AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
		TestEqual(TEXT("Threshold noise does not chatter into recovery"), State.BuildSnapshot().Phase,
			EAuraEncounterPacingPhase::Pressure);
	}
	TestEqual(TEXT("Sustained high intensity enters recovery"), State.Tick(true, 22.0, 0.9, 0.0,
		AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error), EAuraPacingMutationResult::PhaseChanged);
	TestEqual(TEXT("Recovery phase is active"), State.BuildSnapshot().Phase, EAuraEncounterPacingPhase::Recovery);
	TestEqual(TEXT("Low intensity exits only after minimum recovery"), State.Tick(true, 30.0, 0.0, 0.0,
		AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error), EAuraPacingMutationResult::PhaseChanged);
	TestEqual(TEXT("A high sample cannot skip the next minimum build"), State.Tick(true, 30.5, 1.0, 0.0,
		AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error), EAuraPacingMutationResult::NoChange);
	TestEqual(TEXT("Three deliberate phase changes occurred"), State.BuildSnapshot().PhaseChangeCount, 3);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50BudgetAndLiveCaps,
	"Aura.Gameplay.Day50.BudgetAndLiveCaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50BudgetAndLiveCaps::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState MixedState;
	TestTrue(TEXT("Mixed-cost pacing state initializes"), AuraGameplayDay50TestsPrivate::InitializeWithRoster(
		MixedState, AuraGameplayDay50TestsPrivate::MixedRoster(), 1, 50002, 52));
	AuraGameplayDay50TestsPrivate::EnterPressure(MixedState);
	AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(MixedState, 10.0);
	const FAuraEncounterPacingSnapshot MixedSnapshot = MixedState.BuildSnapshot();
	int32 RecomputedMixedCost = 0;
	for (const FAuraPacingRosterSlot& Slot : MixedState.GetRosterSlots())
		if (Slot.bAdmitted && !Slot.bTerminal) RecomputedMixedCost += Slot.Cost;
	TestEqual(TEXT("Mixed archetype costs exactly match the admitted wave cost"),
		MixedSnapshot.CurrentWaveCost, RecomputedMixedCost);
	TestTrue(TEXT("Mixed costs remain inside the solo six-point budget"), MixedSnapshot.CurrentWaveCost <= 6);

	FAuraEncounterPacingState PairState;
	TestTrue(TEXT("Pair pacing state initializes"), AuraGameplayDay50TestsPrivate::Initialize(PairState, 12, 2, 50003, 53));
	AuraGameplayDay50TestsPrivate::EnterPressure(PairState, 2);
	TestEqual(TEXT("Pair wave admits its ten-point budget"),
		AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(PairState, 10.0), 10);
	TestEqual(TEXT("Pair wave cost is bounded at ten"), PairState.BuildSnapshot().CurrentWaveCost, 10);

	FAuraEncounterPacingState State;
	FString Error;
	TestTrue(TEXT("Solo pacing state initializes"), AuraGameplayDay50TestsPrivate::Initialize(State, 12));
	AuraGameplayDay50TestsPrivate::EnterPressure(State);
	TestEqual(TEXT("Solo wave admits exactly its six-point budget"),
		AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(State, 10.0), 6);
	FAuraEncounterPacingSnapshot Snapshot = State.BuildSnapshot();
	TestEqual(TEXT("Wave cost is bounded at six"), Snapshot.CurrentWaveCost, 6);
	TestEqual(TEXT("Six live hostiles remain active"), Snapshot.LiveHostileCount, 6);
	State.Tick(true, 40.0, 0.1, 0.1, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	State.Tick(true, 48.0, 0.1, 0.1, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	State.Tick(true, 58.0, 0.1, 0.1, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	TestEqual(TEXT("Second wave stops at the solo live cap"),
		AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(State, 58.0), 4);
	Snapshot = State.BuildSnapshot();
	TestEqual(TEXT("Live hostiles never exceed ten"), Snapshot.LiveHostileCount, 10);
	TestEqual(TEXT("Live cap can stop a wave below its cost budget"), Snapshot.CurrentWaveCost, 4);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50RecoverySuppressesNewSpawns,
	"Aura.Gameplay.Day50.RecoverySuppressesNewSpawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50RecoverySuppressesNewSpawns::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState State;
	FString Error;
	FName SlotId;
	TestTrue(TEXT("Recovery pacing state initializes"), AuraGameplayDay50TestsPrivate::Initialize(State, 4));
	AuraGameplayDay50TestsPrivate::EnterPressure(State);
	TestEqual(TEXT("One existing hostile is admitted"), State.TryAdmitNext(true, 10.0, SlotId, Error),
		EAuraPacingMutationResult::AdmissionGranted);
	TestEqual(TEXT("High intensity enters recovery"), State.Tick(true, 12.0, 1.0, 0.0,
		AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error), EAuraPacingMutationResult::PhaseChanged);
	TestEqual(TEXT("Recovery defers every new admission"), State.TryAdmitNext(true, 12.0, SlotId, Error),
		EAuraPacingMutationResult::AdmissionDeferred);
	TestEqual(TEXT("Existing hostile continues during recovery"), State.BuildSnapshot().LiveHostileCount, 1);
	TestEqual(TEXT("An open interaction may extend recovery past twenty seconds"), State.Tick(true, 33.0, 1.0, 0.0,
		AuraGameplayDay50TestsPrivate::ConnectedParticipants(), true, Error), EAuraPacingMutationResult::NoChange);
	TestEqual(TEXT("Closing the interaction releases maximum recovery"), State.Tick(true, 33.5, 0.0, 0.0,
		AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error), EAuraPacingMutationResult::PhaseChanged);
	TestEqual(TEXT("Recovery does not rewrite the encounter deadline"), State.BuildSnapshot().DeadlineServerTime, 300.0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50RequiredRosterStillCompletes,
	"Aura.Gameplay.Day50.RequiredRosterStillCompletes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50RequiredRosterStillCompletes::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState State;
	FString Error;
	TArray<FName> FirstWave;
	TestTrue(TEXT("Seven-member required roster initializes"), AuraGameplayDay50TestsPrivate::Initialize(State, 7));
	AuraGameplayDay50TestsPrivate::EnterPressure(State);
	TestEqual(TEXT("First wave admits six required members"),
		AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(State, 10.0, &FirstWave), 6);
	TestTrue(TEXT("First wave can reach terminal outcomes"), AuraGameplayDay50TestsPrivate::MarkTerminal(State, FirstWave));
	State.Tick(true, 40.0, 0.0, 0.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	State.Tick(true, 48.0, 0.0, 0.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	State.Tick(true, 58.0, 0.0, 0.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	FName FinalSlot;
	TestEqual(TEXT("A later pressure wave admits the remaining required member"),
		State.TryAdmitNext(true, 58.0, FinalSlot, Error), EAuraPacingMutationResult::AdmissionGranted);
	TestEqual(TEXT("Final required terminal receipt completes the roster"),
		State.MarkSlotTerminal(true, FinalSlot, 1, 1, Error), EAuraPacingMutationResult::RosterComplete);
	TestTrue(TEXT("Fixed required roster is complete"), State.IsRequiredRosterComplete());
	TestEqual(TEXT("No required admission remains pending"), State.BuildSnapshot().PendingRequiredCount, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50DisconnectNoHealthRewrite,
	"Aura.Gameplay.Day50.DisconnectNoHealthRewrite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50DisconnectNoHealthRewrite::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState State;
	FString Error;
	FName SlotId;
	TestTrue(TEXT("Disconnect pacing state initializes"), AuraGameplayDay50TestsPrivate::Initialize(State, 3));
	AuraGameplayDay50TestsPrivate::EnterPressure(State);
	State.TryAdmitNext(true, 10.0, SlotId, Error);
	FAuraPacingParticipantState Proxy = AuraGameplayDay50TestsPrivate::Participant(
		TEXT("player_a"), false, true, true, 0.35f);
	const TArray<FAuraPacingParticipantState> ProxyParticipants = { Proxy };
	State.Tick(true, 10.5, 0.0, 0.0, ProxyParticipants, false, Error);
	FAuraEncounterPacingSnapshot Snapshot = State.BuildSnapshot();
	TestTrue(TEXT("No connected living participant pauses only new admissions"), Snapshot.bAdmissionPaused);
	TestEqual(TEXT("Dormant proxy remains visible to membership accounting"), Snapshot.LiveDormantProxyCount, 1);
	TestEqual(TEXT("Existing hostile remains live"), Snapshot.LiveHostileCount, 1);
	TestEqual(TEXT("Paused pacing defers a second admission"), State.TryAdmitNext(true, 10.5, SlotId, Error),
		EAuraPacingMutationResult::AdmissionDeferred);
	TestTrue(TEXT("Pacing observes but never mutates proxy health"), FMath::IsNearlyEqual(Proxy.HealthNormalized, 0.35f));
	State.Tick(true, 11.0, 0.0, 0.0, {}, false, Error);
	TestEqual(TEXT("Missing participant input creates no synthetic proxy"), State.BuildSnapshot().LiveDormantProxyCount, 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50HeavyBudgetOwnerPreserved,
	"Aura.Gameplay.Day50.HeavyBudgetOwnerPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50HeavyBudgetOwnerPreserved::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState Pacing;
	FString Error;
	TestTrue(TEXT("Pacing state initializes"), AuraGameplayDay50TestsPrivate::Initialize(Pacing, 3));
	AuraGameplayDay50TestsPrivate::EnterPressure(Pacing);
	UAuraEncounterCoordinator* Coordinator = NewObject<UAuraEncounterCoordinator>();
	TestNotNull(TEXT("Encounter coordinator exists"), Coordinator);
	if (!Coordinator) return false;
	TestTrue(TEXT("Existing encounter coordinator initializes"), Coordinator->Initialize(
		AuraGameplayDay50TestsPrivate::RunId(), 1, 10));
	const TArray<FName> Participants = { TEXT("player_a"), TEXT("player_b") };
	int32 FirstSerial = 0;
	int32 SecondSerial = 0;
	int32 ThirdSerial = 0;
	TestEqual(TEXT("Coordinator owns first heavy admission"), Coordinator->TryAcquireHeavyAttackLease(
		TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), TEXT("enemy_1"), 1, TEXT("heavy_1"), 1,
		Participants, 2, 2, 10.0, 50.0, FirstSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Coordinator owns second heavy admission"), Coordinator->TryAcquireHeavyAttackLease(
		TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), TEXT("enemy_2"), 1, TEXT("heavy_2"), 1,
		Participants, 2, 2, 10.0, 50.0, SecondSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	Pacing.Tick(true, 12.0, 1.0, 0.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, Error);
	TestEqual(TEXT("Pacing recovery does not reset live heavy leases"), Coordinator->GetHeavyAttackLeaseCount(), 2);
	TestEqual(TEXT("Pacing recovery does not reset participant tokens"),
		Coordinator->GetHeavyTokenCountForParticipant(TEXT("player_a")), 2);
	TestEqual(TEXT("Coordinator still rejects a third heavy attack at its cap"),
		Coordinator->TryAcquireHeavyAttackLease(TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"),
			TEXT("enemy_3"), 1, TEXT("heavy_3"), 1, Participants, 2, 2, 13.0, 50.0, ThirdSerial, Error),
		EAuraEncounterAdmissionResult::CapacityFull);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay50SeedAndInputsReproduceDecisions,
	"Aura.Gameplay.Day50.SeedAndInputsReproduceDecisions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay50SeedAndInputsReproduceDecisions::RunTest(const FString& Parameters)
{
	FAuraEncounterPacingState First;
	FAuraEncounterPacingState Second;
	FString FirstError;
	FString SecondError;
	TestTrue(TEXT("First deterministic reducer initializes"), AuraGameplayDay50TestsPrivate::Initialize(First, 8, 1, 771, 51));
	TestTrue(TEXT("Second deterministic reducer initializes"), AuraGameplayDay50TestsPrivate::Initialize(Second, 8, 1, 771, 51));
	First.Tick(true, 10.0, 0.2, 0.2, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, FirstError);
	Second.Tick(true, 10.0, 0.2, 0.2, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, SecondError);
	TArray<FName> FirstWave;
	TArray<FName> SecondWave;
	TestEqual(TEXT("Equal seeds admit equal first-wave counts"),
		AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(First, 10.0, &FirstWave),
		AuraGameplayDay50TestsPrivate::AdmitUntilDeferred(Second, 10.0, &SecondWave));
	TestTrue(TEXT("Equal seeds preserve admission order"), FirstWave == SecondWave);
	First.Tick(true, 10.5, 2.0, -1.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, FirstError);
	Second.Tick(true, 10.5, 2.0, -1.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, SecondError);
	TestTrue(TEXT("Out-of-range signals are deterministically clamped"), First.BuildSnapshot().bLastInputClamped);
	TestTrue(TEXT("Identical seeds and inputs produce identical snapshots"), First.BuildSnapshot() == Second.BuildSnapshot());
	TestTrue(TEXT("Equal terminal receipts remain deterministic"),
		AuraGameplayDay50TestsPrivate::MarkTerminal(First, FirstWave)
		&& AuraGameplayDay50TestsPrivate::MarkTerminal(Second, SecondWave));
	First.Tick(true, 40.0, 0.0, 0.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, FirstError);
	Second.Tick(true, 40.0, 0.0, 0.0, AuraGameplayDay50TestsPrivate::ConnectedParticipants(), false, SecondError);
	TestTrue(TEXT("Phase decisions remain replay-equivalent"), First.BuildSnapshot() == Second.BuildSnapshot());
	const FAuraEncounterPacingSnapshot BeforeRejectedInput = First.BuildSnapshot();
	const TArray<FAuraPacingParticipantState> DuplicateParticipants = {
		AuraGameplayDay50TestsPrivate::Participant(TEXT("player_a")),
		AuraGameplayDay50TestsPrivate::Participant(TEXT("player_a"), false, true, true)
	};
	TestEqual(TEXT("Duplicate participant input is rejected"), First.Tick(
		true, 40.5, 0.8, 0.8, DuplicateParticipants, false, FirstError), EAuraPacingMutationResult::Rejected);
	TestTrue(TEXT("Rejected participant input cannot partially mutate pacing state"),
		First.BuildSnapshot() == BeforeRejectedInput);
	return !HasAnyErrors();
}

#endif
