// Copyright Druid Mechanics

#include "AuraGameplayDay46Harness.h"

#include "Gameplay/AuraEncounterCoordinator.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AuraGameplayDay46HarnessPrivate
{
	void Fail(FString& OutError, const TCHAR* Context, const FString& Detail)
	{
		OutError = FString::Printf(TEXT("%s:%s"), Context, *Detail);
	}

	int32 ResolveGeneration(int32 RequestedGeneration, int32 CurrentGeneration)
	{
		return RequestedGeneration == INDEX_NONE ? CurrentGeneration : RequestedGeneration;
	}

	FString NameOrNone(FName Value)
	{
		return Value.IsNone() ? TEXT("None") : Value.ToString();
	}
}

bool FAuraDay46ContractSnapshot::operator==(const FAuraDay46ContractSnapshot& Other) const
{
	return RunId == Other.RunId && Epoch == Other.Epoch && EncounterId == Other.EncounterId
		&& DriverOwner == Other.DriverOwner && EncounterGeneration == Other.EncounterGeneration
		&& MissionPhase == Other.MissionPhase && MissionRevision == Other.MissionRevision
		&& CurrentCellId == Other.CurrentCellId && CompletedCellCount == Other.CompletedCellCount
		&& bObjectiveComplete == Other.bObjectiveComplete && AttackPhase == Other.AttackPhase
		&& AttackKey == Other.AttackKey && AttackStartServerTime == Other.AttackStartServerTime
		&& AttackExpectedImpactServerTime == Other.AttackExpectedImpactServerTime
		&& AttackDamageWindowEndServerTime == Other.AttackDamageWindowEndServerTime
		&& AttackRecoveryEndServerTime == Other.AttackRecoveryEndServerTime
		&& AttackTerminalReason == Other.AttackTerminalReason
		&& bAttackImpactResolved == Other.bAttackImpactResolved && HeavyLeaseEntries == Other.HeavyLeaseEntries
		&& HeavyTokenEntries == Other.HeavyTokenEntries && SupportLeaseEntries == Other.SupportLeaseEntries
		&& SupportOwnerEntries == Other.SupportOwnerEntries && StatusEntries == Other.StatusEntries
		&& ChannelEntries == Other.ChannelEntries;
}

bool FAuraDay46ContractHarness::Initialize(const FGuid& InRunId, int32 InEpoch, FString& OutError)
{
	OutError.Reset();
	bInitialized = false;
	RunId = InRunId;
	Epoch = InEpoch;
	EncounterId = TEXT("assault_cell_1");
	DriverOwner = TEXT("MissionEncounterDriver");
	EncounterGeneration = 1;
	MissionState = FAuraMissionRunState();
	AttackTimeline = FAuraAttackTimelineState();
	StatusLedger = FAuraCombatStatusLedger();
	EncounterCoordinator = NewObject<UAuraEncounterCoordinator>();
	if (!EncounterCoordinator)
	{
		OutError = TEXT("EncounterCoordinatorCreateFailed");
		return false;
	}
	if (!EncounterCoordinator->Initialize(InRunId, InEpoch, 8))
	{
		OutError = TEXT("EncounterCoordinatorInitializeFailed");
		return false;
	}
	if (!MissionState.TryBeginPreparation(InRunId, InEpoch, TEXT("GameplayExpansionV1"), TEXT("Assault"),
		TEXT("arrangement_a"), 10.0, 1200.0, OutError))
	{
		return false;
	}
	TArray<FAuraMissionCellState> Cells;
	FAuraMissionCellState& FirstCell = Cells.AddDefaulted_GetRef();
	FirstCell.CellId = TEXT("assault_cell_1");
	FirstCell.RequiredDeaths = 2;
	FAuraMissionCellState& SecondCell = Cells.AddDefaulted_GetRef();
	SecondCell.CellId = TEXT("assault_cell_2");
	SecondCell.RequiredDeaths = 2;
	if (!MissionState.ConfigureClearCells(Cells, OutError) || !MissionState.TryActivate(11.0, OutError))
	{
		return false;
	}
	bInitialized = true;
	return true;
}

bool FAuraDay46ContractHarness::ExecutePrimaryScenario(FString& OutError)
{
	OutError.Reset();
	if (!IsInitialized())
	{
		OutError = TEXT("HarnessNotInitialized");
		return false;
	}
	FString Error;
	if (MissionState.RegisterCellDeath(TEXT("assault_cell_1"), TEXT("slot_1"), 1, 1, Error)
		!= EAuraMissionMutationResult::Accepted)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("MissionDeathRejected"), Error);
		return false;
	}

	const FAuraAttackTimelineKey AttackKey = MakeAttackKey(TEXT("raider_1"), 1, 1);
	if (AttackTimeline.TryBeginWindup(AttackKey, MakeAttackDefinition(), 100.0, Error)
		!= EAuraAttackTimelineResult::Started)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("AttackBeginRejected"), Error);
		return false;
	}

	TArray<FName> AffectedParticipants;
	AffectedParticipants.Add(TEXT("player_a"));
	AffectedParticipants.Add(TEXT("player_b"));
	int32 HeavyLeaseSerial = 0;
	if (EncounterCoordinator->TryAcquireHeavyAttackLease(EncounterId, EncounterGeneration, DriverOwner,
		TEXT("raider_1"), 1, TEXT("RaiderHeavyStrike"), 1, AffectedParticipants, 2, 2, 100.0, 101.5,
		HeavyLeaseSerial, Error) != EAuraEncounterAdmissionResult::Acquired)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("HeavyLeaseAcquireRejected"), Error);
		return false;
	}

	int32 SupportLeaseSerial = 0;
	if (EncounterCoordinator->TryAcquireSupportFieldLease(EncounterId, EncounterGeneration, DriverOwner,
		TEXT("disruptor_1"), 1, TEXT("DisruptorSupportField"), AffectedParticipants, 2, 100.0, 102.0,
		SupportLeaseSerial, Error) != EAuraEncounterAdmissionResult::Acquired)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("SupportLeaseAcquireRejected"), Error);
		return false;
	}

	const FAuraCombatStatusKey StatusKey = MakeStatusKey(TEXT("disruptor_1"), 1, TEXT("player_a"), 1,
		TEXT("Day46SyntheticExposed"), 1);
	if (StatusLedger.TryApplyStatus(StatusKey, TEXT("UniquePerTarget"), TEXT("SupportField"), 100.0, 3.0, Error)
		!= EAuraCombatStatusResult::Applied)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("StatusApplyRejected"), Error);
		return false;
	}

	const FAuraInterruptChannelKey ChannelKey = MakeChannelKey(TEXT("disruptor_1"), 1,
		TEXT("DisruptorSupportChannel"), 1);
	if (!StatusLedger.TryBeginInterruptibleChannel(ChannelKey, 100.0, 2.0, Error))
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("ChannelBeginRejected"), Error);
		return false;
	}
	if (AttackTimeline.TryCommit(AttackKey, 100.85, Error) != EAuraAttackTimelineResult::Committed
		|| AttackTimeline.TryResolveImpact(AttackKey, 100.85, Error) != EAuraAttackTimelineResult::ImpactResolved)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("AttackImpactRejected"), Error);
		return false;
	}
	if (StatusLedger.TryInterruptChannel(ChannelKey, 1, 100.5, Error) != EAuraInterruptResult::Interrupted)
	{
		AuraGameplayDay46HarnessPrivate::Fail(OutError, TEXT("ChannelInterruptRejected"), Error);
		return false;
	}
	return true;
}

bool FAuraDay46ContractHarness::AdvanceEncounterGeneration(int32 NewGeneration, FString& OutError)
{
	OutError.Reset();
	if (!IsInitialized() || NewGeneration <= EncounterGeneration)
	{
		OutError = TEXT("EncounterGenerationAdvanceRejected");
		return false;
	}
	EncounterGeneration = NewGeneration;
	return true;
}

FAuraAttackTimelineKey FAuraDay46ContractHarness::MakeAttackKey(FName SourceEntityId, int32 SourceLifeGeneration,
	int32 AttackSequence, int32 InEncounterGeneration, FName InDriverOwner) const
{
	FAuraAttackTimelineKey Key;
	Key.RunId = RunId;
	Key.Epoch = Epoch;
	Key.EncounterGeneration = AuraGameplayDay46HarnessPrivate::ResolveGeneration(InEncounterGeneration, EncounterGeneration);
	Key.EncounterId = EncounterId;
	Key.DriverOwner = InDriverOwner.IsNone() ? DriverOwner : InDriverOwner;
	Key.SourceEntityId = SourceEntityId;
	Key.SourceLifeGeneration = SourceLifeGeneration;
	Key.AttackSequence = AttackSequence;
	return Key;
}

FAuraAttackTimelineDefinition FAuraDay46ContractHarness::MakeAttackDefinition() const
{
	FAuraAttackTimelineDefinition Definition;
	Definition.AttackDefinitionId = TEXT("RaiderHeavyStrike");
	Definition.AttackShapeId = TEXT("shape_raider_heavy_arc");
	Definition.TelegraphProfileId = TEXT("telegraph_heavy_arc");
	Definition.WindupSeconds = 0.85f;
	Definition.DamageWindowSeconds = 0.05f;
	Definition.RecoverySeconds = 1.0f;
	Definition.bShapeFrozenDuringWindup = true;
	Definition.bUsesServerFallbackTiming = true;
	return Definition;
}

FAuraCombatStatusKey FAuraDay46ContractHarness::MakeStatusKey(FName SourceEntityId, int32 SourceLifeGeneration,
	FName TargetEntityId, int32 TargetLifeGeneration, FName StatusDefinitionId, int32 ApplicationSequence,
	int32 InEncounterGeneration) const
{
	FAuraCombatStatusKey Key;
	Key.RunId = RunId;
	Key.Epoch = Epoch;
	Key.EncounterGeneration = AuraGameplayDay46HarnessPrivate::ResolveGeneration(InEncounterGeneration, EncounterGeneration);
	Key.SourceEntityId = SourceEntityId;
	Key.SourceLifeGeneration = SourceLifeGeneration;
	Key.TargetEntityId = TargetEntityId;
	Key.TargetLifeGeneration = TargetLifeGeneration;
	Key.StatusDefinitionId = StatusDefinitionId;
	Key.ApplicationSequence = ApplicationSequence;
	return Key;
}

FAuraInterruptChannelKey FAuraDay46ContractHarness::MakeChannelKey(FName TargetEntityId, int32 TargetLifeGeneration,
	FName ChannelId, int32 ChannelGeneration, int32 InEncounterGeneration) const
{
	FAuraInterruptChannelKey Key;
	Key.RunId = RunId;
	Key.Epoch = Epoch;
	Key.EncounterGeneration = AuraGameplayDay46HarnessPrivate::ResolveGeneration(InEncounterGeneration, EncounterGeneration);
	Key.TargetEntityId = TargetEntityId;
	Key.TargetLifeGeneration = TargetLifeGeneration;
	Key.ChannelId = ChannelId;
	Key.ChannelGeneration = ChannelGeneration;
	return Key;
}

FString FAuraDay46ContractHarness::MakeAttackKeyEntry(const FAuraAttackTimelineKey& Key)
{
	return FString::Printf(TEXT("%s|%d|%d|%s|%s|%s|%d|%d"), *Key.RunId.ToString(), Key.Epoch,
		Key.EncounterGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Key.EncounterId),
		*AuraGameplayDay46HarnessPrivate::NameOrNone(Key.DriverOwner),
		*AuraGameplayDay46HarnessPrivate::NameOrNone(Key.SourceEntityId), Key.SourceLifeGeneration, Key.AttackSequence);
}

FString FAuraDay46ContractHarness::MakeStatusKeyEntry(const FAuraCombatStatusKey& Key)
{
	return FString::Printf(TEXT("%s|%d|%d|%s|%d|%s|%d|%s|%d"), *Key.RunId.ToString(), Key.Epoch,
		Key.EncounterGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Key.SourceEntityId),
		Key.SourceLifeGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Key.TargetEntityId),
		Key.TargetLifeGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Key.StatusDefinitionId),
		Key.ApplicationSequence);
}

FString FAuraDay46ContractHarness::MakeChannelKeyEntry(const FAuraInterruptChannelKey& Key)
{
	return FString::Printf(TEXT("%s|%d|%d|%s|%d|%s|%d"), *Key.RunId.ToString(), Key.Epoch,
		Key.EncounterGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Key.TargetEntityId),
		Key.TargetLifeGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Key.ChannelId), Key.ChannelGeneration);
}

FString FAuraDay46ContractHarness::MakeParticipantList(const TArray<FName>& Participants)
{
	TArray<FString> Names;
	for (const FName Participant : Participants) Names.Add(AuraGameplayDay46HarnessPrivate::NameOrNone(Participant));
	Names.Sort();
	return FString::Join(Names, TEXT(","));
}

FAuraDay46ContractSnapshot FAuraDay46ContractHarness::BuildSnapshot() const
{
	FAuraDay46ContractSnapshot Snapshot;
	Snapshot.RunId = RunId;
	Snapshot.Epoch = Epoch;
	Snapshot.EncounterId = EncounterId;
	Snapshot.DriverOwner = DriverOwner;
	Snapshot.EncounterGeneration = EncounterGeneration;
	const FAuraMissionPublicSnapshot MissionSnapshot = MissionState.BuildPublicSnapshot();
	Snapshot.MissionPhase = MissionSnapshot.Phase;
	Snapshot.MissionRevision = MissionSnapshot.Revision;
	Snapshot.CurrentCellId = MissionSnapshot.CurrentCellId;
	Snapshot.CompletedCellCount = MissionSnapshot.CompletedCellCount;
	Snapshot.bObjectiveComplete = MissionSnapshot.bObjectiveComplete;
	Snapshot.AttackPhase = AttackTimeline.Phase;
	Snapshot.AttackKey = AttackTimeline.Key;
	Snapshot.AttackStartServerTime = AttackTimeline.AuthorityStartServerTime;
	Snapshot.AttackExpectedImpactServerTime = AttackTimeline.ExpectedImpactServerTime;
	Snapshot.AttackDamageWindowEndServerTime = AttackTimeline.DamageWindowEndServerTime;
	Snapshot.AttackRecoveryEndServerTime = AttackTimeline.RecoveryEndServerTime;
	Snapshot.AttackTerminalReason = AttackTimeline.TerminalReason;
	Snapshot.bAttackImpactResolved = AttackTimeline.bImpactResolved;

	TArray<FName> HeavyParticipants;
	for (const FAuraHeavyAttackLease& Lease : EncounterCoordinator->GetHeavyAttackLeases())
	{
		Snapshot.HeavyLeaseEntries.Add(FString::Printf(TEXT("%s|%d|%s|%d|%s|%s|%d|%s|%d|%.6f|%s"),
			*Lease.RunId.ToString(), Lease.Epoch, *AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.EncounterId),
			Lease.EncounterGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.DriverOwner),
			*AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.SourceEntityId), Lease.SourceLifeGeneration,
			*AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.AttackId), Lease.LeaseSerial, Lease.ExpiryServerTime,
			*MakeParticipantList(Lease.AffectedParticipants)));
		for (const FName Participant : Lease.AffectedParticipants) HeavyParticipants.AddUnique(Participant);
	}
	HeavyParticipants.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});
	for (const FName Participant : HeavyParticipants)
	{
		Snapshot.HeavyTokenEntries.Add(FString::Printf(TEXT("%s=%d"), *Participant.ToString(),
			EncounterCoordinator->GetHeavyTokenCountForParticipant(Participant)));
	}

	TMap<FString, int32> SupportOwnerSerials;
	for (const FAuraSupportFieldLease& Lease : EncounterCoordinator->GetSupportFieldLeases())
	{
		Snapshot.SupportLeaseEntries.Add(FString::Printf(TEXT("%s|%d|%s|%d|%s|%s|%d|%s|%d|%.6f|%d|%s"),
			*Lease.RunId.ToString(), Lease.Epoch, *AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.EncounterId),
			Lease.EncounterGeneration, *AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.DriverOwner),
			*AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.SourceEntityId), Lease.SourceLifeGeneration,
			*AuraGameplayDay46HarnessPrivate::NameOrNone(Lease.FieldId), Lease.LeaseSerial, Lease.ChannelEndServerTime,
			Lease.MaxAffectedParticipants, *MakeParticipantList(Lease.AffectedParticipants)));
		for (const FName Participant : Lease.AffectedParticipants)
		{
			const FString OwnerKey = FString::Printf(TEXT("%s|%d|%s"), *Lease.EncounterId.ToString(),
				Lease.EncounterGeneration, *Participant.ToString());
			SupportOwnerSerials.FindOrAdd(OwnerKey) = EncounterCoordinator->GetEffectiveSupportFieldLeaseSerial(
				Lease.EncounterId, Lease.EncounterGeneration, Participant);
		}
	}
	for (const TPair<FString, int32>& Owner : SupportOwnerSerials)
	{
		Snapshot.SupportOwnerEntries.Add(FString::Printf(TEXT("%s=%d"), *Owner.Key, Owner.Value));
	}

	for (const FAuraCombatStatusRecord& Record : StatusLedger.GetStatuses())
	{
		Snapshot.StatusEntries.Add(FString::Printf(TEXT("%s|%s|%s|%.6f|%.6f"), *MakeStatusKeyEntry(Record.Key),
			*AuraGameplayDay46HarnessPrivate::NameOrNone(Record.StackingPolicy),
			*AuraGameplayDay46HarnessPrivate::NameOrNone(Record.InterruptClassification), Record.StartServerTime,
			Record.ExpiryServerTime));
	}
	for (const FAuraInterruptChannelState& Channel : StatusLedger.GetChannels())
	{
		Snapshot.ChannelEntries.Add(FString::Printf(TEXT("%s|%.6f|%.6f|%.6f|%d|%d"),
			*MakeChannelKeyEntry(Channel.Key), Channel.StartServerTime, Channel.EndServerTime,
			Channel.InterruptImmuneUntilServerTime, Channel.LastInterruptSequence, Channel.bActive ? 1 : 0));
	}

	Snapshot.HeavyLeaseEntries.Sort();
	Snapshot.HeavyTokenEntries.Sort();
	Snapshot.SupportLeaseEntries.Sort();
	Snapshot.SupportOwnerEntries.Sort();
	Snapshot.StatusEntries.Sort();
	Snapshot.ChannelEntries.Sort();
	return Snapshot;
}

#endif
