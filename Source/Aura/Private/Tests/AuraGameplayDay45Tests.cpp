// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Gameplay/AuraCombatStatusTypes.h"
#include "Gameplay/AuraEncounterCoordinator.h"

namespace AuraGameplayDay45TestsPrivate
{
	TArray<FName> Participants(const TCHAR* First, const TCHAR* Second = nullptr)
	{
		TArray<FName> Result;
		Result.Add(FName(First));
		if (Second) Result.Add(FName(Second));
		return Result;
	}

	EAuraEncounterAdmissionResult AcquireHeavy(UAuraEncounterCoordinator& Coordinator, FName SourceEntityId,
		int32 SourceLifeGeneration, FName AttackId, const TArray<FName>& AffectedParticipants, double Expiry,
		int32& OutSerial, FString& OutError)
	{
		return Coordinator.TryAcquireHeavyAttackLease(TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"),
			SourceEntityId, SourceLifeGeneration, AttackId, 1, AffectedParticipants, 2, 2, 10.0, Expiry, OutSerial, OutError);
	}

	EAuraEncounterAdmissionResult AcquireSupport(UAuraEncounterCoordinator& Coordinator, FName SourceEntityId,
		int32 SourceLifeGeneration, FName FieldId, const TArray<FName>& AffectedParticipants, int32 MaxAffectedParticipants,
		double ChannelEnd, int32& OutSerial, FString& OutError)
	{
		return Coordinator.TryAcquireSupportFieldLease(TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"),
			SourceEntityId, SourceLifeGeneration, FieldId, AffectedParticipants, MaxAffectedParticipants, 10.0,
			ChannelEnd, OutSerial, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay45MixedGroupThreatCap,
	"Aura.Gameplay.Day45.MixedGroupThreatCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay45MixedGroupThreatCap::RunTest(const FString& Parameters)
{
	UAuraEncounterCoordinator* Coordinator = NewObject<UAuraEncounterCoordinator>();
	TestNotNull(TEXT("Encounter coordinator exists"), Coordinator);
	if (!Coordinator) return false;
	TestTrue(TEXT("Coordinator initializes for the test run"), Coordinator->Initialize(FGuid(1, 2, 3, 4), 1, 8));
	FString Error;
	int32 FirstSerial = 0;
	const TArray<FName> TwoParticipants = AuraGameplayDay45TestsPrivate::Participants(TEXT("player_a"), TEXT("player_b"));
	TestEqual(TEXT("First heavy attack acquires every required participant token"),
		AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, TEXT("enemy_1"), 1, TEXT("attack_1"),
			TwoParticipants, 20.0, FirstSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Each participant owns one token"), Coordinator->GetHeavyTokenCountForParticipant(TEXT("player_a")), 1);
	TArray<FName> ReorderedParticipants = AuraGameplayDay45TestsPrivate::Participants(TEXT("player_b"), TEXT("player_a"));
	int32 DuplicateSerial = 0;
	TestEqual(TEXT("Replaying the same acquire is idempotent"),
		AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, TEXT("enemy_1"), 1, TEXT("attack_1"),
			ReorderedParticipants, 20.0, DuplicateSerial, Error), EAuraEncounterAdmissionResult::DuplicateAccepted);
	TestEqual(TEXT("Idempotent replay returns the original serial"), DuplicateSerial, FirstSerial);

	int32 SecondSerial = 0;
	TestEqual(TEXT("The second concurrent heavy attack is admitted below the global cap"),
		AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, TEXT("enemy_2"), 1, TEXT("attack_2"),
			TwoParticipants, 20.0, SecondSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	int32 ThirdSerial = 0;
	TestEqual(TEXT("A third heavy attack is rejected at the global cap"),
		AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, TEXT("enemy_3"), 1, TEXT("attack_3"),
			AuraGameplayDay45TestsPrivate::Participants(TEXT("player_c")), 20.0, ThirdSerial, Error),
		EAuraEncounterAdmissionResult::CapacityFull);
	TestEqual(TEXT("Failed multi-target admission does not partially consume a token"),
		Coordinator->GetHeavyTokenCountForParticipant(TEXT("player_c")), 0);
	TestFalse(TEXT("Wrong driver cannot release a heavy lease"), Coordinator->ReleaseHeavyAttackLease(TEXT("cell_1"), 1,
		TEXT("LegacyDriver"), TEXT("enemy_1"), 1, TEXT("attack_1"), 1, FirstSerial, Error));
	TestFalse(TEXT("Old source life cannot release a replacement enemy lease"), Coordinator->ReleaseHeavyAttackLease(
		TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), TEXT("enemy_1"), 2, TEXT("attack_1"), 1, FirstSerial, Error));
	TestTrue(TEXT("The original source can release its lease exactly once"), Coordinator->ReleaseHeavyAttackLease(
		TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), TEXT("enemy_1"), 1, TEXT("attack_1"), 1, FirstSerial, Error));
	TestFalse(TEXT("A released heavy lease cannot be released twice"), Coordinator->ReleaseHeavyAttackLease(
		TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), TEXT("enemy_1"), 1, TEXT("attack_1"), 1, FirstSerial, Error));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay45HeavyLeaseStaleGeneration,
	"Aura.Gameplay.Day45.HeavyLeaseStaleGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay45HeavyLeaseStaleGeneration::RunTest(const FString& Parameters)
{
	UAuraEncounterCoordinator* Coordinator = NewObject<UAuraEncounterCoordinator>();
	TestNotNull(TEXT("Encounter coordinator exists"), Coordinator);
	if (!Coordinator) return false;
	Coordinator->Initialize(FGuid(1, 2, 3, 4), 1, 8);
	FString Error;
	int32 Serial = 0;
	TestEqual(TEXT("Generation-one lease is admitted"), Coordinator->TryAcquireHeavyAttackLease(TEXT("cell_1"), 1,
		TEXT("MissionEncounterDriver"), TEXT("enemy_1"), 1, TEXT("attack_1"),
		1, AuraGameplayDay45TestsPrivate::Participants(TEXT("player_a")), 2, 2, 10.0, 20.0, Serial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Driver teardown revokes the generation lease"),
		Coordinator->InvalidateEncounterGeneration(TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), Error), 1);
	TestEqual(TEXT("Generation teardown leaves no heavy capacity occupied"), Coordinator->GetHeavyAttackLeaseCount(), 0);
	TestEqual(TEXT("An expired lease can be admitted before its deadline"), Coordinator->TryAcquireHeavyAttackLease(TEXT("cell_1"), 2,
		TEXT("MissionEncounterDriver"), TEXT("enemy_2"), 1, TEXT("attack_2"),
		1, AuraGameplayDay45TestsPrivate::Participants(TEXT("player_a")), 2, 2, 10.0, 11.0, Serial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Expired heavy lease is removed exactly once"), Coordinator->PruneExpiredLeases(11.0), 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay45SupportFieldNoStack,
	"Aura.Gameplay.Day45.SupportFieldNoStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay45SupportFieldNoStack::RunTest(const FString& Parameters)
{
	UAuraEncounterCoordinator* Coordinator = NewObject<UAuraEncounterCoordinator>();
	TestNotNull(TEXT("Encounter coordinator exists"), Coordinator);
	if (!Coordinator) return false;
	Coordinator->Initialize(FGuid(1, 2, 3, 4), 1, 8);
	FString Error;
	int32 FirstSerial = 0;
	int32 SecondSerial = 0;
	TestEqual(TEXT("First Disruptor field is admitted"), Coordinator->TryAcquireSupportFieldLease(TEXT("cell_1"), 1,
		TEXT("MissionEncounterDriver"), TEXT("disruptor_1"), 1, TEXT("DisruptorSupportField"),
		AuraGameplayDay45TestsPrivate::Participants(TEXT("player_a"), TEXT("player_b")), 2, 10.0, 12.0,
		FirstSerial, Error), EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Second overlapping field is admitted as a separate source lease"), Coordinator->TryAcquireSupportFieldLease(
		TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), TEXT("disruptor_2"), 1, TEXT("DisruptorSupportField"),
		AuraGameplayDay45TestsPrivate::Participants(TEXT("player_a")), 2, 10.0, 12.0, SecondSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);
	TestEqual(TEXT("Lowest live source lease owns the overlapping effect"),
		Coordinator->GetEffectiveSupportFieldLeaseSerial(TEXT("cell_1"), 1, TEXT("player_a")), FirstSerial);
	TestFalse(TEXT("Stale source life cannot remove a support field"), Coordinator->ReleaseSupportFieldLease(TEXT("cell_1"), 1,
		TEXT("MissionEncounterDriver"), TEXT("disruptor_1"), 2, TEXT("DisruptorSupportField"), FirstSerial,
		EAuraSupportFieldTerminationReason::OwnerDeath, Error));
	TestTrue(TEXT("Source death removes only its own field"), Coordinator->ReleaseSupportFieldLease(TEXT("cell_1"), 1,
		TEXT("MissionEncounterDriver"), TEXT("disruptor_1"), 1, TEXT("DisruptorSupportField"), FirstSerial,
		EAuraSupportFieldTerminationReason::OwnerDeath, Error));
	TestEqual(TEXT("Ownership reevaluates to the remaining live source"),
		Coordinator->GetEffectiveSupportFieldLeaseSerial(TEXT("cell_1"), 1, TEXT("player_a")), SecondSerial);
	TestTrue(TEXT("Range departure updates only the source coverage"), Coordinator->UpdateSupportFieldCoverage(TEXT("cell_1"), 1,
		TEXT("MissionEncounterDriver"), TEXT("disruptor_2"), 1, TEXT("DisruptorSupportField"), SecondSerial,
		AuraGameplayDay45TestsPrivate::Participants(TEXT("player_b")), Error));
	TestEqual(TEXT("Departed participant no longer receives the field"),
		Coordinator->GetEffectiveSupportFieldLeaseSerial(TEXT("cell_1"), 1, TEXT("player_a")), 0);
	TestEqual(TEXT("Remaining participant receives the same source field"),
		Coordinator->GetEffectiveSupportFieldLeaseSerial(TEXT("cell_1"), 1, TEXT("player_b")), SecondSerial);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay45ActiveLeaseReplaySurvivesHistoryEviction,
	"Aura.Gameplay.Day45.ActiveLeaseReplaySurvivesHistoryEviction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay45ActiveLeaseReplaySurvivesHistoryEviction::RunTest(const FString& Parameters)
{
	UAuraEncounterCoordinator* Coordinator = NewObject<UAuraEncounterCoordinator>();
	TestNotNull(TEXT("Encounter coordinator exists"), Coordinator);
	if (!Coordinator) return false;
	Coordinator->Initialize(FGuid(1, 2, 3, 4), 1, 8);
	FString Error;
	const TArray<FName> Participants = AuraGameplayDay45TestsPrivate::Participants(TEXT("player_a"));
	int32 ActiveHeavySerial = 0;
	TestEqual(TEXT("The active heavy lease is admitted"), AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator,
		TEXT("active_enemy"), 1, TEXT("active_attack"), Participants, 100.0, ActiveHeavySerial, Error),
		EAuraEncounterAdmissionResult::Acquired);

	bool bHeavyHistoryFloodSucceeded = true;
	for (int32 Index = 0; Index < 1024; ++Index)
	{
		const FString SourceName = FString::Printf(TEXT("history_enemy_%d"), Index);
		const FString AttackName = FString::Printf(TEXT("history_attack_%d"), Index);
		const FName SourceId(*SourceName);
		const FName AttackId(*AttackName);
		int32 HistorySerial = 0;
		if (AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, SourceId, 1, AttackId, Participants, 20.0,
			HistorySerial, Error) != EAuraEncounterAdmissionResult::Acquired
			|| !Coordinator->ReleaseHeavyAttackLease(TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), SourceId, 1,
				AttackId, 1, HistorySerial, Error))
		{
			bHeavyHistoryFloodSucceeded = false;
			break;
		}
	}
	TestTrue(TEXT("Terminal heavy request history can exceed its bounded cache"), bHeavyHistoryFloodSucceeded);

	int32 ReplayHeavySerial = 0;
	TestEqual(TEXT("An active heavy lease remains idempotent after history eviction"),
		AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, TEXT("active_enemy"), 1, TEXT("active_attack"),
			Participants, 100.0, ReplayHeavySerial, Error), EAuraEncounterAdmissionResult::DuplicateAccepted);
	TestEqual(TEXT("Active heavy replay returns the original serial"), ReplayHeavySerial, ActiveHeavySerial);
	TestEqual(TEXT("Active heavy replay does not consume another lease"), Coordinator->GetHeavyAttackLeaseCount(), 1);
	int32 ConflictingHeavySerial = 0;
	TestEqual(TEXT("An active heavy identity still rejects a changed fingerprint after history eviction"),
		AuraGameplayDay45TestsPrivate::AcquireHeavy(*Coordinator, TEXT("active_enemy"), 1, TEXT("active_attack"),
			Participants, 101.0, ConflictingHeavySerial, Error), EAuraEncounterAdmissionResult::Conflict);
	TestEqual(TEXT("Heavy identity conflict does not allocate a second lease"), Coordinator->GetHeavyAttackLeaseCount(), 1);

	int32 ActiveSupportSerial = 0;
	const TArray<FName> SupportParticipants = AuraGameplayDay45TestsPrivate::Participants(TEXT("player_b"));
	TestEqual(TEXT("The active support field is admitted"), AuraGameplayDay45TestsPrivate::AcquireSupport(*Coordinator,
		TEXT("active_disruptor"), 1, TEXT("active_field"), SupportParticipants, 2, 100.0, ActiveSupportSerial, Error),
		EAuraEncounterAdmissionResult::Acquired);

	bool bSupportHistoryFloodSucceeded = true;
	for (int32 Index = 0; Index < 1024; ++Index)
	{
		const FString SourceName = FString::Printf(TEXT("history_disruptor_%d"), Index);
		const FString FieldName = FString::Printf(TEXT("history_field_%d"), Index);
		const FName SourceId(*SourceName);
		const FName FieldId(*FieldName);
		int32 HistorySerial = 0;
		if (AuraGameplayDay45TestsPrivate::AcquireSupport(*Coordinator, SourceId, 1, FieldId, SupportParticipants, 2,
			20.0, HistorySerial, Error) != EAuraEncounterAdmissionResult::Acquired
			|| !Coordinator->ReleaseSupportFieldLease(TEXT("cell_1"), 1, TEXT("MissionEncounterDriver"), SourceId, 1,
				FieldId, HistorySerial, EAuraSupportFieldTerminationReason::Cancelled, Error))
		{
			bSupportHistoryFloodSucceeded = false;
			break;
		}
	}
	TestTrue(TEXT("Terminal support request history can exceed its bounded cache"), bSupportHistoryFloodSucceeded);

	int32 ReplaySupportSerial = 0;
	TestEqual(TEXT("An active support field remains idempotent after history eviction"),
		AuraGameplayDay45TestsPrivate::AcquireSupport(*Coordinator, TEXT("active_disruptor"), 1, TEXT("active_field"),
			SupportParticipants, 2, 100.0, ReplaySupportSerial, Error), EAuraEncounterAdmissionResult::DuplicateAccepted);
	TestEqual(TEXT("Active support replay returns the original serial"), ReplaySupportSerial, ActiveSupportSerial);
	TestEqual(TEXT("Active support replay does not consume another lease"), Coordinator->GetSupportFieldLeaseCount(), 1);
	int32 ConflictingSupportSerial = 0;
	TestEqual(TEXT("An active support identity still rejects a changed fingerprint after history eviction"),
		AuraGameplayDay45TestsPrivate::AcquireSupport(*Coordinator, TEXT("active_disruptor"), 1, TEXT("active_field"),
			SupportParticipants, 2, 101.0, ConflictingSupportSerial, Error), EAuraEncounterAdmissionResult::Conflict);
	TestEqual(TEXT("Support identity conflict does not allocate a second lease"), Coordinator->GetSupportFieldLeaseCount(), 1);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraGameplayDay45DisruptorInterruptCleanup,
	"Aura.Gameplay.Day45.DisruptorInterruptCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraGameplayDay45DisruptorInterruptCleanup::RunTest(const FString& Parameters)
{
	FAuraCombatStatusLedger Ledger;
	FAuraCombatStatusKey StatusKey;
	StatusKey.RunId = FGuid(1, 2, 3, 4);
	StatusKey.Epoch = 1;
	StatusKey.EncounterGeneration = 1;
	StatusKey.SourceEntityId = TEXT("disruptor_1");
	StatusKey.SourceLifeGeneration = 1;
	StatusKey.TargetEntityId = TEXT("player_a");
	StatusKey.TargetLifeGeneration = 1;
	StatusKey.StatusDefinitionId = TEXT("DisruptorSupportReduction");
	StatusKey.ApplicationSequence = 1;
	FString Error;
	TestEqual(TEXT("Support status applies once"), Ledger.TryApplyStatus(StatusKey, TEXT("UniquePerTarget"),
		TEXT("SupportField"), 10.0, 2.0, Error), EAuraCombatStatusResult::Applied);
	TestEqual(TEXT("Support status replay is idempotent"), Ledger.TryApplyStatus(StatusKey, TEXT("UniquePerTarget"),
		TEXT("SupportField"), 10.0, 2.0, Error), EAuraCombatStatusResult::DuplicateAccepted);
	FAuraCombatStatusKey StaleStatusKey = StatusKey;
	StaleStatusKey.SourceLifeGeneration = 2;
	TestEqual(TEXT("Replacement source cannot remove the old status"), Ledger.TryRemoveStatus(StaleStatusKey,
		EAuraCombatStatusRemovalReason::OwnerDeath, Error), EAuraCombatStatusResult::Rejected);
	TestEqual(TEXT("Owner death removes the exact status record"), Ledger.TryRemoveStatus(StatusKey,
		EAuraCombatStatusRemovalReason::OwnerDeath, Error), EAuraCombatStatusResult::Removed);

	FAuraInterruptChannelKey ChannelKey;
	ChannelKey.RunId = StatusKey.RunId;
	ChannelKey.Epoch = 1;
	ChannelKey.EncounterGeneration = 1;
	ChannelKey.TargetEntityId = TEXT("disruptor_1");
	ChannelKey.TargetLifeGeneration = 1;
	ChannelKey.ChannelId = TEXT("DisruptorSupportChannel");
	ChannelKey.ChannelGeneration = 1;
	TestTrue(TEXT("Interruptible support channel begins"), Ledger.TryBeginInterruptibleChannel(ChannelKey, 10.0, 2.0, Error));
	TestEqual(TEXT("Accepted interrupt ends the channel"), Ledger.TryInterruptChannel(ChannelKey, 1, 10.5, Error),
		EAuraInterruptResult::Interrupted);
	FAuraInterruptChannelKey ReplacementChannelKey = ChannelKey;
	ReplacementChannelKey.ChannelGeneration = 2;
	TestFalse(TEXT("Two-second channel immunity blocks an immediate replacement"),
		Ledger.TryBeginInterruptibleChannel(ReplacementChannelKey, 10.6, 2.0, Error));
	TestTrue(TEXT("Channel can begin after immunity expires"), Ledger.TryBeginInterruptibleChannel(ReplacementChannelKey, 12.6, 2.0, Error));
	TestEqual(TEXT("Repeated interrupt delivery is idempotently classified"), Ledger.TryInterruptChannel(ReplacementChannelKey, 1, 12.7, Error),
		EAuraInterruptResult::Interrupted);
	return !HasAnyErrors();
}

#endif
