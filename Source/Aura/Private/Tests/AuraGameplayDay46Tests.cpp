// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraEncounterCoordinator.h"
#include "AuraGameplayDay46Harness.h"

namespace AuraGameplayDay46TestsPrivate
{
	FGuid TestRunId()
	{
		return FGuid(46, 2026, 9, 3);
	}

	bool Initialize(FAuraDay46ContractHarness& Harness, FString& OutError)
	{
		return Harness.Initialize(TestRunId(), 1, OutError);
	}

	TArray<FName> Participants(FName First, FName Second = NAME_None)
	{
		TArray<FName> Result;
		Result.Add(First);
		if (!Second.IsNone()) Result.Add(Second);
		return Result;
	}

	EAuraEncounterAdmissionResult AcquireHeavy(FAuraDay46ContractHarness& Harness, int32 Generation,
		FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId, const TArray<FName>& AffectedParticipants,
		double Now, double Expiry, int32& OutSerial, FString& OutError)
	{
		return Harness.GetEncounterCoordinator().TryAcquireHeavyAttackLease(Harness.GetEncounterId(), Generation,
			Harness.GetDriverOwner(), SourceEntityId, SourceLifeGeneration, AttackId, 1, AffectedParticipants, 2, 2, Now,
			Expiry, OutSerial, OutError);
	}

	EAuraEncounterAdmissionResult AcquireSupport(FAuraDay46ContractHarness& Harness, int32 Generation,
		FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId, const TArray<FName>& AffectedParticipants,
		int32 MaxAffectedParticipants, double Now, double EndServerTime, int32& OutSerial, FString& OutError)
	{
		return Harness.GetEncounterCoordinator().TryAcquireSupportFieldLease(Harness.GetEncounterId(), Generation,
			Harness.GetDriverOwner(), SourceEntityId, SourceLifeGeneration, FieldId, AffectedParticipants,
			MaxAffectedParticipants, Now, EndServerTime, OutSerial, OutError);
	}

	bool HasActiveChannel(const FAuraCombatStatusLedger& Ledger, const FAuraInterruptChannelKey& Key)
	{
		for (const FAuraInterruptChannelState& Channel : Ledger.GetChannels())
		{
			if (Channel.Key == Key && Channel.bActive) return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46DeterministicReplay,
	"Aura.Gameplay.Day46.DeterministicReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46DeterministicReplay::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness FirstHarness;
	FAuraDay46ContractHarness ReplayHarness;
	FString Error;
	TestTrue(TEXT("First deterministic harness initializes"),
		AuraGameplayDay46TestsPrivate::Initialize(FirstHarness, Error));
	TestTrue(TEXT("Replay deterministic harness initializes"),
		AuraGameplayDay46TestsPrivate::Initialize(ReplayHarness, Error));
	TestTrue(TEXT("First authority event stream completes"), FirstHarness.ExecutePrimaryScenario(Error));
	TestTrue(TEXT("Replay authority event stream completes"), ReplayHarness.ExecutePrimaryScenario(Error));
	TestTrue(TEXT("Equivalent authority streams produce the same value snapshot"),
		FirstHarness.BuildSnapshot() == ReplayHarness.BuildSnapshot());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46EncounterGenerationBarrier,
	"Aura.Gameplay.Day46.EncounterGenerationBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46EncounterGenerationBarrier::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const TArray<FName> AffectedParticipants = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"));
	int32 OldHeavySerial = 0;
	int32 OldSupportSerial = 0;
	TestEqual(TEXT("Generation-one heavy lease is admitted"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_1"), 1, TEXT("old_heavy"), AffectedParticipants, 10.0, 50.0, OldHeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Generation-one support lease is admitted"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_1"), 1, TEXT("old_support"), AffectedParticipants, 2, 10.0, 50.0, OldSupportSerial,
		Error), EAuraEncounterAdmissionResult::Acquired);
	const FAuraCombatStatusKey OldStatus = Harness.MakeStatusKey(TEXT("disruptor_1"), 1, TEXT("player_a"), 1,
		TEXT("Day46GenerationStatus"), 1, 1);
	TestEqual(TEXT("Generation-one status is admitted"), Harness.GetStatusLedger().TryApplyStatus(OldStatus,
		TEXT("UniquePerTarget"), TEXT("SupportField"), 10.0, 20.0, Error), EAuraCombatStatusResult::Applied);
	const FAuraInterruptChannelKey OldChannel = Harness.MakeChannelKey(TEXT("player_a"), 1,
		TEXT("Day46GenerationChannel"), 1, 1);
	TestTrue(TEXT("Generation-one channel is admitted"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(
		OldChannel, 10.0, 20.0, Error));

	TestEqual(TEXT("Generation invalidation removes only generation-one leases"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 2);
	TestEqual(TEXT("Status ledger remains independently observable until its caller cleans it"),
		Harness.GetStatusLedger().GetStatuses().Num(), 1);
	TestEqual(TEXT("Channel ledger remains independently observable until its caller cleans it"),
		Harness.GetStatusLedger().GetChannels().Num(), 1);
	TestTrue(TEXT("Harness advances to the replacement generation"), Harness.AdvanceEncounterGeneration(2, Error));

	int32 NewHeavySerial = 0;
	int32 NewSupportSerial = 0;
	TestEqual(TEXT("Generation-two heavy lease is admitted"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 2,
		TEXT("raider_1"), 1, TEXT("new_heavy"), AffectedParticipants, 20.0, 50.0, NewHeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Generation-two support lease is admitted"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 2,
		TEXT("disruptor_1"), 1, TEXT("new_support"), AffectedParticipants, 2, 20.0, 50.0, NewSupportSerial,
		Error), EAuraEncounterAdmissionResult::Acquired);
	const FAuraCombatStatusKey NewStatus = Harness.MakeStatusKey(TEXT("disruptor_1"), 1, TEXT("player_a"), 1,
		TEXT("Day46GenerationStatus"), 1, 2);
	TestEqual(TEXT("Generation-two status has a distinct identity"), Harness.GetStatusLedger().TryApplyStatus(NewStatus,
		TEXT("UniquePerTarget"), TEXT("SupportField"), 20.0, 20.0, Error), EAuraCombatStatusResult::Applied);
	const FAuraInterruptChannelKey NewChannel = Harness.MakeChannelKey(TEXT("player_a"), 1,
		TEXT("Day46GenerationChannel"), 1, 2);
	TestTrue(TEXT("Generation-two channel has a distinct identity"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(
		NewChannel, 20.0, 20.0, Error));

	TestFalse(TEXT("A stale heavy release cannot remove the replacement generation"), Coordinator.ReleaseHeavyAttackLease(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("raider_1"), 1, TEXT("old_heavy"), 1, OldHeavySerial,
		Error));
	TestFalse(TEXT("A stale support release cannot remove the replacement generation"), Coordinator.ReleaseSupportFieldLease(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("disruptor_1"), 1, TEXT("old_support"),
		OldSupportSerial, EAuraSupportFieldTerminationReason::EncounterInvalidated, Error));
	TestEqual(TEXT("Exact old status cleanup does not remove the replacement status"), Harness.GetStatusLedger().TryRemoveStatus(
		OldStatus, EAuraCombatStatusRemovalReason::EncounterInvalidated, Error), EAuraCombatStatusResult::Removed);
	TestEqual(TEXT("Old channel cleanup is isolated from the replacement channel"), Harness.GetStatusLedger().TryInterruptChannel(
		OldChannel, 1, 20.5, Error), EAuraInterruptResult::Interrupted);
	TestTrue(TEXT("Replacement channel remains active after stale-generation cleanup"),
		AuraGameplayDay46TestsPrivate::HasActiveChannel(Harness.GetStatusLedger(), NewChannel));
	TestEqual(TEXT("Replacement status is the only remaining status"), Harness.GetStatusLedger().GetStatuses().Num(), 1);
	TestEqual(TEXT("Replacement support field remains the effective owner"),
		Coordinator.GetEffectiveSupportFieldLeaseSerial(Harness.GetEncounterId(), 2, TEXT("player_a")), NewSupportSerial);
	TestEqual(TEXT("Repeated generation invalidation is idempotent"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46SourceLifeBarrier,
	"Aura.Gameplay.Day46.SourceLifeBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46SourceLifeBarrier::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));

	const FAuraAttackTimelineKey LiveAttack = Harness.MakeAttackKey(TEXT("raider_1"), 1, 1);
	TestEqual(TEXT("Live source attack begins"), Harness.GetAttackTimeline().TryBeginWindup(LiveAttack,
		Harness.MakeAttackDefinition(), 100.0, Error), EAuraAttackTimelineResult::Started);
	const FAuraAttackTimelineKey ReplacementLifeAttack = Harness.MakeAttackKey(TEXT("raider_1"), 2, 1);
	TestEqual(TEXT("Replacement source life cannot commit the old attack"), Harness.GetAttackTimeline().TryCommit(
		ReplacementLifeAttack, 100.85, Error), EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Stale attack callback leaves the live source in windup"), Harness.GetAttackTimeline().Phase,
		EAuraAttackTimelinePhase::Windup);

	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const TArray<FName> AffectedParticipants = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"));
	int32 HeavySerial = 0;
	int32 SupportSerial = 0;
	TestEqual(TEXT("Live source heavy lease is admitted"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_1"), 1, TEXT("source_life_heavy"), AffectedParticipants, 100.0, 120.0, HeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Live source support lease is admitted"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_1"), 1, TEXT("source_life_support"), AffectedParticipants, 2, 100.0, 120.0, SupportSerial,
		Error), EAuraEncounterAdmissionResult::Acquired);
	TestFalse(TEXT("Replacement source life cannot release the heavy lease"), Coordinator.ReleaseHeavyAttackLease(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("raider_1"), 2, TEXT("source_life_heavy"), 1,
		HeavySerial, Error));
	TestFalse(TEXT("Replacement source life cannot update the support lease"), Coordinator.UpdateSupportFieldCoverage(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("disruptor_1"), 2, TEXT("source_life_support"),
		SupportSerial, AffectedParticipants, Error));
	TestEqual(TEXT("Stale lease callbacks leave both live leases present"), Coordinator.GetHeavyAttackLeaseCount()
		+ Coordinator.GetSupportFieldLeaseCount(), 2);

	const FAuraCombatStatusKey LiveStatus = Harness.MakeStatusKey(TEXT("disruptor_1"), 1, TEXT("player_a"), 1,
		TEXT("Day46SourceLifeStatus"), 1);
	const FAuraCombatStatusKey ReplacementStatus = Harness.MakeStatusKey(TEXT("disruptor_1"), 2, TEXT("player_a"), 1,
		TEXT("Day46SourceLifeStatus"), 1);
	TestEqual(TEXT("Live source status is admitted"), Harness.GetStatusLedger().TryApplyStatus(LiveStatus,
		TEXT("UniquePerTarget"), TEXT("SupportField"), 100.0, 5.0, Error), EAuraCombatStatusResult::Applied);
	TestEqual(TEXT("Replacement source life cannot remove the live status"), Harness.GetStatusLedger().TryRemoveStatus(
		ReplacementStatus, EAuraCombatStatusRemovalReason::OwnerDeath, Error), EAuraCombatStatusResult::Rejected);
	TestEqual(TEXT("Live source status remains after stale removal"), Harness.GetStatusLedger().GetStatuses().Num(), 1);

	const FAuraInterruptChannelKey LiveChannel = Harness.MakeChannelKey(TEXT("disruptor_1"), 1,
		TEXT("Day46SourceLifeChannel"), 1);
	const FAuraInterruptChannelKey ReplacementChannel = Harness.MakeChannelKey(TEXT("disruptor_1"), 2,
		TEXT("Day46SourceLifeChannel"), 1);
	TestTrue(TEXT("Live source channel is admitted"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(
		LiveChannel, 100.0, 5.0, Error));
	TestTrue(TEXT("Replacement source life may own a distinct channel identity"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(
		ReplacementChannel, 100.0, 5.0, Error));
	TestEqual(TEXT("Replacement source life interrupts only its own channel"), Harness.GetStatusLedger().TryInterruptChannel(
		ReplacementChannel, 1, 100.5, Error), EAuraInterruptResult::Interrupted);
	TestTrue(TEXT("Live channel remains independently active"),
		AuraGameplayDay46TestsPrivate::HasActiveChannel(Harness.GetStatusLedger(), LiveChannel));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46DriverOwnershipBarrier,
	"Aura.Gameplay.Day46.DriverOwnershipBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46DriverOwnershipBarrier::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const FName LegacyDriver(TEXT("LegacyBehaviorDriver"));

	TestTrue(TEXT("Canonical driver reserves the encounter slot"), Coordinator.TryReserveSlot(Harness.GetEncounterId(),
		TEXT("slot_1"), 1, Harness.GetDriverOwner(), 1, Error));
	TestFalse(TEXT("Legacy driver cannot commit the reserved slot"), Coordinator.CommitSlot(TEXT("slot_1"), 1,
		LegacyDriver, 1, Error));
	TestFalse(TEXT("Legacy driver cannot release the pending slot"), Coordinator.ReleaseSlot(TEXT("slot_1"), 1,
		LegacyDriver, 1, Error));
	TestTrue(TEXT("Canonical driver commits the reserved slot"), Coordinator.CommitSlot(TEXT("slot_1"), 1,
		Harness.GetDriverOwner(), 1, Error));
	TestFalse(TEXT("Legacy driver cannot release the live slot"), Coordinator.ReleaseSlot(TEXT("slot_1"), 1,
		LegacyDriver, 1, Error));
	TestTrue(TEXT("Canonical driver releases the live slot"), Coordinator.ReleaseSlot(TEXT("slot_1"), 1,
		Harness.GetDriverOwner(), 1, Error));

	const TArray<FName> AffectedParticipants = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"));
	int32 HeavySerial = 0;
	int32 SupportSerial = 0;
	TestEqual(TEXT("Canonical driver owns the heavy lease"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_1"), 1, TEXT("driver_heavy"), AffectedParticipants, 120.0, 140.0, HeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Canonical driver owns the support lease"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_1"), 1, TEXT("driver_support"), AffectedParticipants, 2, 120.0, 140.0, SupportSerial,
		Error), EAuraEncounterAdmissionResult::Acquired);
	TestFalse(TEXT("Legacy driver cannot release the heavy lease"), Coordinator.ReleaseHeavyAttackLease(
		Harness.GetEncounterId(), 1, LegacyDriver, TEXT("raider_1"), 1, TEXT("driver_heavy"), 1, HeavySerial, Error));
	TestFalse(TEXT("Legacy driver cannot update the support lease"), Coordinator.UpdateSupportFieldCoverage(
		Harness.GetEncounterId(), 1, LegacyDriver, TEXT("disruptor_1"), 1, TEXT("driver_support"), SupportSerial,
		AffectedParticipants, Error));

	const FAuraAttackTimelineKey LiveAttack = Harness.MakeAttackKey(TEXT("raider_2"), 1, 1);
	const FAuraAttackTimelineKey WrongDriverAttack = Harness.MakeAttackKey(TEXT("raider_2"), 1, 1, 1, LegacyDriver);
	TestEqual(TEXT("Canonical driver starts the attack timeline"), Harness.GetAttackTimeline().TryBeginWindup(LiveAttack,
		Harness.MakeAttackDefinition(), 120.0, Error), EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("Legacy driver callback cannot commit the canonical timeline"), Harness.GetAttackTimeline().TryCommit(
		WrongDriverAttack, 120.85, Error), EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Canonical timeline remains in windup after driver rejection"), Harness.GetAttackTimeline().Phase,
		EAuraAttackTimelinePhase::Windup);
	TestEqual(TEXT("Canonical driver can cancel its own timeline"), Harness.GetAttackTimeline().TryCancel(LiveAttack,
		TEXT("DriverTeardown"), Error), EAuraAttackTimelineResult::Cancelled);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46HeavyLeaseLifecycleConvergence,
	"Aura.Gameplay.Day46.HeavyLeaseLifecycleConvergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46HeavyLeaseLifecycleConvergence::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const TArray<FName> TwoParticipants = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"), TEXT("player_b"));
	int32 FirstSerial = 0;
	TestEqual(TEXT("Initial heavy lease acquires all participant tokens"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness,
		1, TEXT("raider_1"), 1, TEXT("lifecycle_attack"), TwoParticipants, 10.0, 20.0, FirstSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	int32 DuplicateSerial = 0;
	const TArray<FName> ReorderedParticipants = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_b"), TEXT("player_a"));
	TestEqual(TEXT("Reordered replay is idempotent"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_1"), 1, TEXT("lifecycle_attack"), ReorderedParticipants, 10.0, 20.0, DuplicateSerial, Error),
		EAuraEncounterAdmissionResult::DuplicateAccepted);
	TestEqual(TEXT("Reordered replay returns the original serial"), DuplicateSerial, FirstSerial);
	int32 ConflictSerial = 0;
	TestEqual(TEXT("Changed active payload is a conflict"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_1"), 1, TEXT("lifecycle_attack"), TwoParticipants, 10.0, 21.0, ConflictSerial, Error),
		EAuraEncounterAdmissionResult::Conflict);
	TestEqual(TEXT("Conflict does not allocate another heavy lease"), Coordinator.GetHeavyAttackLeaseCount(), 1);
	TestFalse(TEXT("Wrong driver cannot release the lease"), Coordinator.ReleaseHeavyAttackLease(Harness.GetEncounterId(), 1,
		TEXT("LegacyBehaviorDriver"), TEXT("raider_1"), 1, TEXT("lifecycle_attack"), 1, FirstSerial, Error));
	TestTrue(TEXT("Owner release removes the lease"), Coordinator.ReleaseHeavyAttackLease(Harness.GetEncounterId(), 1,
		Harness.GetDriverOwner(), TEXT("raider_1"), 1, TEXT("lifecycle_attack"), 1, FirstSerial, Error));
	TestFalse(TEXT("Repeated owner release is idempotently rejected"), Coordinator.ReleaseHeavyAttackLease(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("raider_1"), 1, TEXT("lifecycle_attack"), 1, FirstSerial,
		Error));
	TestEqual(TEXT("Released heavy tokens are fully returned"), Coordinator.GetHeavyTokenCountForParticipant(TEXT("player_a")), 0);
	int32 ReacquiredSerial = 0;
	TestEqual(TEXT("The same attack definition can be admitted as a new authority instance"),
		Coordinator.TryAcquireHeavyAttackLease(Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("raider_1"), 1,
			TEXT("lifecycle_attack"), 2, TwoParticipants, 2, 2, 22.0, 30.0, ReacquiredSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestTrue(TEXT("The new attack instance has a distinct lease serial"), ReacquiredSerial != FirstSerial);

	int32 ExpiringSerial = 0;
	TestEqual(TEXT("Expiring lease acquires before its deadline"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_expiring"), 1, TEXT("expiring_attack"), AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a")),
		30.0, 31.0, ExpiringSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Lease remains live before its deadline"), Coordinator.PruneExpiredLeases(30.999), 0);
	TestEqual(TEXT("Lease is removed at its deadline exactly once"), Coordinator.PruneExpiredLeases(31.0), 1);
	TestEqual(TEXT("Second expiry pass is a no-op"), Coordinator.PruneExpiredLeases(31.0), 0);

	int32 GenerationSerial = 0;
	TestEqual(TEXT("Replacement generation lease acquires"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 2,
		TEXT("raider_generation"), 1, TEXT("generation_attack"), AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a")),
		40.0, 50.0, GenerationSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Generation invalidation removes the live replacement lease"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 2, Harness.GetDriverOwner(), Error), 1);
	TestEqual(TEXT("Repeated generation invalidation converges to zero"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 2, Harness.GetDriverOwner(), Error), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46SupportOwnerPromotion,
	"Aura.Gameplay.Day46.SupportOwnerPromotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46SupportOwnerPromotion::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const TArray<FName> Participant = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"));
	int32 FirstSerial = 0;
	int32 SecondSerial = 0;
	int32 ThirdSerial = 0;
	TestEqual(TEXT("First support source acquires"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_1"), 1, TEXT("field_1"), Participant, 2, 10.0, 50.0, FirstSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Second overlapping support source acquires"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_2"), 1, TEXT("field_2"), Participant, 2, 10.0, 50.0, SecondSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Third overlapping support source acquires"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_3"), 1, TEXT("field_3"), Participant, 2, 10.0, 50.0, ThirdSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Lowest live serial is the effective owner"), Coordinator.GetEffectiveSupportFieldLeaseSerial(
		Harness.GetEncounterId(), 1, TEXT("player_a")), FirstSerial);
	TestTrue(TEXT("First source release promotes the second source"), Coordinator.ReleaseSupportFieldLease(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("disruptor_1"), 1, TEXT("field_1"), FirstSerial,
		EAuraSupportFieldTerminationReason::OwnerDeath, Error));
	TestEqual(TEXT("Second live serial is promoted"), Coordinator.GetEffectiveSupportFieldLeaseSerial(Harness.GetEncounterId(),
		1, TEXT("player_a")), SecondSerial);
	TestTrue(TEXT("Second source release promotes the third source"), Coordinator.ReleaseSupportFieldLease(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("disruptor_2"), 1, TEXT("field_2"), SecondSerial,
		EAuraSupportFieldTerminationReason::RangeDeparture, Error));
	TestEqual(TEXT("Third live serial is promoted"), Coordinator.GetEffectiveSupportFieldLeaseSerial(Harness.GetEncounterId(),
		1, TEXT("player_a")), ThirdSerial);
	TestTrue(TEXT("Coverage update removes the departed participant"), Coordinator.UpdateSupportFieldCoverage(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), TEXT("disruptor_3"), 1, TEXT("field_3"), ThirdSerial,
		AuraGameplayDay46TestsPrivate::Participants(TEXT("player_b")), Error));
	TestEqual(TEXT("Departed participant no longer has effective support"), Coordinator.GetEffectiveSupportFieldLeaseSerial(
		Harness.GetEncounterId(), 1, TEXT("player_a")), 0);
	TestEqual(TEXT("Updated participant receives the remaining support"), Coordinator.GetEffectiveSupportFieldLeaseSerial(
		Harness.GetEncounterId(), 1, TEXT("player_b")), ThirdSerial);
	TestEqual(TEXT("Generation cleanup removes the remaining source"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 1);
	TestEqual(TEXT("Support ownership is empty after cleanup"), Coordinator.GetSupportFieldLeaseCount(), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46AttackReplacementIsolation,
	"Aura.Gameplay.Day46.AttackReplacementIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46AttackReplacementIsolation::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	const FAuraAttackTimelineKey OldAttack = Harness.MakeAttackKey(TEXT("raider_1"), 1, 1, 1);
	TestEqual(TEXT("Old attack begins"), Harness.GetAttackTimeline().TryBeginWindup(OldAttack,
		Harness.MakeAttackDefinition(), 200.0, Error), EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("Old attack cancellation is terminal and observable"), Harness.GetAttackTimeline().TryCancel(OldAttack,
		TEXT("SourceDeath"), Error), EAuraAttackTimelineResult::Cancelled);
	Harness.GetAttackTimeline().Reset();
	TestTrue(TEXT("Harness advances the attack replacement generation"), Harness.AdvanceEncounterGeneration(2, Error));
	const FAuraAttackTimelineKey NewAttack = Harness.MakeAttackKey(TEXT("raider_1"), 1, 1, 2);
	TestEqual(TEXT("Replacement attack begins after explicit reset"), Harness.GetAttackTimeline().TryBeginWindup(NewAttack,
		Harness.MakeAttackDefinition(), 200.0, Error), EAuraAttackTimelineResult::Started);
	TestEqual(TEXT("Late old commit is rejected"), Harness.GetAttackTimeline().TryCommit(OldAttack, 200.85, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Late old impact is rejected"), Harness.GetAttackTimeline().TryResolveImpact(OldAttack, 200.85, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Late old recovery callback is rejected"), Harness.GetAttackTimeline().TryFinishRecovery(OldAttack, 202.0,
		Error), EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Replacement remains in windup after stale callbacks"), Harness.GetAttackTimeline().Phase,
		EAuraAttackTimelinePhase::Windup);
	TestEqual(TEXT("Replacement commits on its own authoritative deadline"), Harness.GetAttackTimeline().TryCommit(NewAttack,
		200.85, Error), EAuraAttackTimelineResult::Committed);
	TestEqual(TEXT("Replacement resolves its own impact"), Harness.GetAttackTimeline().TryResolveImpact(NewAttack, 200.85,
		Error), EAuraAttackTimelineResult::ImpactResolved);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46TerminalCleanupIdempotence,
	"Aura.Gameplay.Day46.TerminalCleanupIdempotence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46TerminalCleanupIdempotence::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	FAuraMissionRunState& Mission = Harness.GetMissionState();
	TestEqual(TEXT("First mission death is accepted"), Mission.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_1"),
		1, 1, Error), EAuraMissionMutationResult::Accepted);
	TestEqual(TEXT("Second first-cell death emits one boundary"), Mission.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_2"),
		1, 1, Error), EAuraMissionMutationResult::CellCompleted);
	TestEqual(TEXT("First second-cell death is accepted"), Mission.RegisterCellDeath(TEXT("assault_cell_2"), TEXT("slot_3"),
		1, 1, Error), EAuraMissionMutationResult::Accepted);
	TestEqual(TEXT("Second second-cell death completes the objective"), Mission.RegisterCellDeath(TEXT("assault_cell_2"), TEXT("slot_4"),
		1, 1, Error), EAuraMissionMutationResult::ObjectiveCompleted);
	TestTrue(TEXT("Extraction begins only after the objective boundary"), Mission.TryBeginExtraction(300.0, 30.0, Error));
	TestTrue(TEXT("Settlement opens once"), Mission.TryBeginSettlement(true, Error));
	TestTrue(TEXT("Settlement commits once"), Mission.TryCommitSettlement(Error));
	const int32 TerminalRevision = Mission.Revision;
	TestEqual(TEXT("Mission reaches the completed phase"), Mission.Phase, EAuraMissionPhase::Completed);
	TestFalse(TEXT("Duplicate settlement commit is rejected"), Mission.TryCommitSettlement(Error));
	TestFalse(TEXT("Late failure cannot overwrite the completed mission"), Mission.TryFail(TEXT("late_failure"), Error));
	TestFalse(TEXT("Late abort cannot overwrite the completed mission"), Mission.TryAbort(TEXT("late_abort"), Error));
	TestEqual(TEXT("Terminal mission revision is stable"), Mission.Revision, TerminalRevision);

	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const TArray<FName> Participant = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"));
	int32 HeavySerial = 0;
	int32 SupportSerial = 0;
	TestEqual(TEXT("Terminal cleanup starts with a heavy lease"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_terminal"), 1, TEXT("terminal_heavy"), Participant, 10.0, 20.0, HeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Terminal cleanup starts with a support lease"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_terminal"), 1, TEXT("terminal_support"), Participant, 2, 10.0, 20.0, SupportSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	const FAuraCombatStatusKey StatusKey = Harness.MakeStatusKey(TEXT("disruptor_terminal"), 1, TEXT("player_a"), 1,
		TEXT("Day46TerminalStatus"), 1);
	TestEqual(TEXT("Terminal status is present before cleanup"), Harness.GetStatusLedger().TryApplyStatus(StatusKey,
		TEXT("UniquePerTarget"), TEXT("SupportField"), 10.0, 2.0, Error), EAuraCombatStatusResult::Applied);
	const FAuraInterruptChannelKey ChannelKey = Harness.MakeChannelKey(TEXT("player_a"), 1,
		TEXT("Day46TerminalChannel"), 1);
	TestTrue(TEXT("Terminal channel is present before cleanup"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(
		ChannelKey, 10.0, 2.0, Error));
	TestEqual(TEXT("Encounter cleanup removes both live leases once"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 2);
	TestEqual(TEXT("Repeated encounter cleanup removes nothing"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 0);
	TestEqual(TEXT("Expired status and channel cleanup is one pass"), Harness.GetStatusLedger().PruneExpired(12.0), 2);
	TestEqual(TEXT("Repeated status cleanup is a no-op"), Harness.GetStatusLedger().PruneExpired(12.0), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay46CrossContractStaleStorm,
	"Aura.Gameplay.Day46.CrossContractStaleStorm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay46CrossContractStaleStorm::RunTest(const FString& Parameters)
{
	FAuraDay46ContractHarness Harness;
	FString Error;
	TestTrue(TEXT("Harness initializes"), AuraGameplayDay46TestsPrivate::Initialize(Harness, Error));
	UAuraEncounterCoordinator& Coordinator = Harness.GetEncounterCoordinator();
	const TArray<FName> Participant = AuraGameplayDay46TestsPrivate::Participants(TEXT("player_a"));

	const FAuraAttackTimelineKey OldAttack = Harness.MakeAttackKey(TEXT("raider_storm"), 1, 1, 1);
	TestEqual(TEXT("Old attack begins"), Harness.GetAttackTimeline().TryBeginWindup(OldAttack,
		Harness.MakeAttackDefinition(), 10.0, Error), EAuraAttackTimelineResult::Started);
	int32 OldHeavySerial = 0;
	int32 OldSupportSerial = 0;
	TestEqual(TEXT("Old heavy lease begins"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 1,
		TEXT("raider_storm"), 1, TEXT("storm_old_heavy"), Participant, 10.0, 100.0, OldHeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Old support lease begins"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 1,
		TEXT("disruptor_storm"), 1, TEXT("storm_old_support"), Participant, 2, 10.0, 100.0, OldSupportSerial,
		Error), EAuraEncounterAdmissionResult::Acquired);
	const FAuraCombatStatusKey OldStatus = Harness.MakeStatusKey(TEXT("disruptor_storm"), 1, TEXT("player_a"), 1,
		TEXT("Day46StormStatus"), 1, 1);
	TestEqual(TEXT("Old status begins"), Harness.GetStatusLedger().TryApplyStatus(OldStatus, TEXT("UniquePerTarget"),
		TEXT("SupportField"), 10.0, 100.0, Error), EAuraCombatStatusResult::Applied);
	const FAuraInterruptChannelKey OldChannel = Harness.MakeChannelKey(TEXT("player_a"), 1,
		TEXT("Day46StormChannel"), 1, 1);
	TestTrue(TEXT("Old channel begins"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(OldChannel, 10.0,
		100.0, Error));

	TestEqual(TEXT("Old leases are invalidated before replacement"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 2);
	TestEqual(TEXT("Old status is removed before replacement"), Harness.GetStatusLedger().TryRemoveStatus(OldStatus,
		EAuraCombatStatusRemovalReason::EncounterInvalidated, Error), EAuraCombatStatusResult::Removed);
	TestEqual(TEXT("Old channel is terminated before replacement"), Harness.GetStatusLedger().TryInterruptChannel(OldChannel,
		1, 10.5, Error), EAuraInterruptResult::Interrupted);
	TestEqual(TEXT("Cancelled old timeline is reset explicitly"), Harness.GetAttackTimeline().TryCancel(OldAttack,
		TEXT("EncounterInvalidated"), Error), EAuraAttackTimelineResult::Cancelled);
	Harness.GetAttackTimeline().Reset();
	TestTrue(TEXT("Replacement generation advances"), Harness.AdvanceEncounterGeneration(2, Error));

	const FAuraAttackTimelineKey NewAttack = Harness.MakeAttackKey(TEXT("raider_storm"), 1, 1, 2);
	TestEqual(TEXT("Replacement attack begins"), Harness.GetAttackTimeline().TryBeginWindup(NewAttack,
		Harness.MakeAttackDefinition(), 20.0, Error), EAuraAttackTimelineResult::Started);
	int32 NewHeavySerial = 0;
	int32 NewSupportSerial = 0;
	TestEqual(TEXT("Replacement heavy lease begins"), AuraGameplayDay46TestsPrivate::AcquireHeavy(Harness, 2,
		TEXT("raider_storm"), 1, TEXT("storm_new_heavy"), Participant, 20.0, 100.0, NewHeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Replacement support lease begins"), AuraGameplayDay46TestsPrivate::AcquireSupport(Harness, 2,
		TEXT("disruptor_storm"), 1, TEXT("storm_new_support"), Participant, 2, 20.0, 100.0, NewSupportSerial,
		Error), EAuraEncounterAdmissionResult::Acquired);
	const FAuraCombatStatusKey NewStatus = Harness.MakeStatusKey(TEXT("disruptor_storm"), 1, TEXT("player_a"), 1,
		TEXT("Day46StormStatus"), 1, 2);
	TestEqual(TEXT("Replacement status begins"), Harness.GetStatusLedger().TryApplyStatus(NewStatus, TEXT("UniquePerTarget"),
		TEXT("SupportField"), 20.0, 100.0, Error), EAuraCombatStatusResult::Applied);
	const FAuraInterruptChannelKey NewChannel = Harness.MakeChannelKey(TEXT("player_a"), 1,
		TEXT("Day46StormChannel"), 1, 2);
	TestTrue(TEXT("Replacement channel begins"), Harness.GetStatusLedger().TryBeginInterruptibleChannel(NewChannel, 20.0,
		100.0, Error));
	const FAuraDay46ContractSnapshot StableSnapshot = Harness.BuildSnapshot();

	TestFalse(TEXT("Stale heavy release is rejected"), Coordinator.ReleaseHeavyAttackLease(Harness.GetEncounterId(), 1,
		Harness.GetDriverOwner(), TEXT("raider_storm"), 1, TEXT("storm_old_heavy"), 1, OldHeavySerial, Error));
	TestFalse(TEXT("Stale support release is rejected"), Coordinator.ReleaseSupportFieldLease(Harness.GetEncounterId(), 1,
		Harness.GetDriverOwner(), TEXT("disruptor_storm"), 1, TEXT("storm_old_support"), OldSupportSerial,
		EAuraSupportFieldTerminationReason::EncounterInvalidated, Error));
	TestEqual(TEXT("Stale status removal is rejected after prior cleanup"), Harness.GetStatusLedger().TryRemoveStatus(
		OldStatus, EAuraCombatStatusRemovalReason::Manual, Error), EAuraCombatStatusResult::Rejected);
	TestEqual(TEXT("Stale channel callback is rejected after prior cleanup"), Harness.GetStatusLedger().TryInterruptChannel(
		OldChannel, 2, 20.5, Error), EAuraInterruptResult::Rejected);
	TestEqual(TEXT("Stale attack commit is rejected"), Harness.GetAttackTimeline().TryCommit(OldAttack, 20.85, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Stale attack impact is rejected"), Harness.GetAttackTimeline().TryResolveImpact(OldAttack, 20.85, Error),
		EAuraAttackTimelineResult::Rejected);
	TestEqual(TEXT("Repeated generation invalidation is a no-op"), Coordinator.InvalidateEncounterGeneration(
		Harness.GetEncounterId(), 1, Harness.GetDriverOwner(), Error), 0);
	TestTrue(TEXT("Stale callback storm leaves the replacement snapshot unchanged"), StableSnapshot == Harness.BuildSnapshot());
	TestTrue(TEXT("Replacement channel remains active after stale callbacks"),
		AuraGameplayDay46TestsPrivate::HasActiveChannel(Harness.GetStatusLedger(), NewChannel));
	TestEqual(TEXT("Replacement support owner remains stable"), Coordinator.GetEffectiveSupportFieldLeaseSerial(
		Harness.GetEncounterId(), 2, TEXT("player_a")), NewSupportSerial);
	TestEqual(TEXT("Replacement heavy token remains stable"), Coordinator.GetHeavyTokenCountForParticipant(TEXT("player_a")), 1);
	return !HasAnyErrors();
}

#endif
