// Copyright Druid Mechanics

#include "Gameplay/AuraEncounterCoordinator.h"

namespace AuraEncounterCoordinatorPrivate
{
	constexpr int32 MaxRequestHistory = 1024;

	bool ValidateParticipants(const TArray<FName>& Participants, int32 Maximum, TArray<FName>& OutCanonical,
		FString& OutError)
	{
		OutCanonical.Reset();
		if (Maximum <= 0 || Participants.Num() > Maximum)
		{
			OutError = TEXT("ParticipantCapacityInvalid");
			return false;
		}
		TArray<FString> Names;
		for (const FName Participant : Participants)
		{
			if (Participant.IsNone())
			{
				OutError = TEXT("ParticipantIdentityInvalid");
				return false;
			}
			Names.Add(Participant.ToString());
		}
		Names.Sort();
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			if (Index > 0 && Names[Index] == Names[Index - 1])
			{
				OutError = TEXT("DuplicateParticipant");
				return false;
			}
			OutCanonical.Add(FName(*Names[Index]));
		}
		return true;
	}
}

bool UAuraEncounterCoordinator::Initialize(const FGuid& InRunId, int32 InEpoch, int32 InMaxLiveHostiles)
{
	if (!InRunId.IsValid() || InEpoch <= 0 || InMaxLiveHostiles <= 0) return false;
	Reset();
	RunId = InRunId;
	Epoch = InEpoch;
	MaxLiveHostiles = InMaxLiveHostiles;
	return true;
}

int32 UAuraEncounterCoordinator::FindLeaseIndex(FName SlotId) const
{
	for (int32 Index = 0; Index < Leases.Num(); ++Index)
	{
		if (Leases[Index].SlotId == SlotId) return Index;
	}
	return INDEX_NONE;
}

int32 UAuraEncounterCoordinator::FindHeavyLeaseIndex(int32 LeaseSerial) const
{
	for (int32 Index = 0; Index < HeavyAttackLeases.Num(); ++Index)
	{
		if (HeavyAttackLeases[Index].LeaseSerial == LeaseSerial) return Index;
	}
	return INDEX_NONE;
}

int32 UAuraEncounterCoordinator::FindActiveHeavyLeaseIndex(const FString& IdentityKey) const
{
	for (int32 Index = 0; Index < HeavyAttackLeases.Num(); ++Index)
	{
		const FAuraHeavyAttackLease& Lease = HeavyAttackLeases[Index];
		if (MakeHeavyIdentityKey(Lease.RunId, Lease.Epoch, Lease.EncounterId, Lease.EncounterGeneration,
			Lease.DriverOwner, Lease.SourceEntityId, Lease.SourceLifeGeneration, Lease.AttackId,
			Lease.AttackSequence) == IdentityKey)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UAuraEncounterCoordinator::FindSupportFieldLeaseIndex(int32 LeaseSerial) const
{
	for (int32 Index = 0; Index < SupportFieldLeases.Num(); ++Index)
	{
		if (SupportFieldLeases[Index].LeaseSerial == LeaseSerial) return Index;
	}
	return INDEX_NONE;
}

int32 UAuraEncounterCoordinator::FindActiveSupportFieldLeaseIndex(const FString& IdentityKey) const
{
	for (int32 Index = 0; Index < SupportFieldLeases.Num(); ++Index)
	{
		const FAuraSupportFieldLease& Lease = SupportFieldLeases[Index];
		if (MakeSupportIdentityKey(Lease.RunId, Lease.Epoch, Lease.EncounterId, Lease.EncounterGeneration,
			Lease.DriverOwner, Lease.SourceEntityId, Lease.SourceLifeGeneration, Lease.FieldId) == IdentityKey)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UAuraEncounterCoordinator::GetPendingCount() const
{
	int32 Count = 0;
	for (const FAuraEncounterSlotLease& Lease : Leases) if (Lease.bPending) ++Count;
	return Count;
}

int32 UAuraEncounterCoordinator::GetLiveCount() const
{
	int32 Count = 0;
	for (const FAuraEncounterSlotLease& Lease : Leases) if (Lease.bLive) ++Count;
	return Count;
}

bool UAuraEncounterCoordinator::TryReserveSlot(FName EncounterId, FName SlotId, int32 Generation, FName DriverOwner,
	int32 SourceLifeGeneration, FString& OutError)
{
	OutError.Reset();
	if (!RunId.IsValid() || Epoch <= 0 || EncounterId.IsNone() || SlotId.IsNone() || Generation <= 0
		|| DriverOwner.IsNone() || SourceLifeGeneration <= 0)
	{
		OutError = TEXT("InvalidLeaseIdentity");
		return false;
	}
	if (FindLeaseIndex(SlotId) != INDEX_NONE)
	{
		OutError = TEXT("DuplicateSlot");
		return false;
	}
	if (Leases.Num() >= MaxLiveHostiles)
	{
		OutError = TEXT("HostileCapacityFull");
		return false;
	}
	FAuraEncounterSlotLease& Lease = Leases.AddDefaulted_GetRef();
	Lease.RunId = RunId;
	Lease.Epoch = Epoch;
	Lease.EncounterId = EncounterId;
	Lease.SlotId = SlotId;
	Lease.Generation = Generation;
	Lease.SourceLifeGeneration = SourceLifeGeneration;
	Lease.DriverOwner = DriverOwner;
	Lease.bPending = true;
	Lease.bLive = false;
	return true;
}

bool UAuraEncounterCoordinator::CommitSlot(FName SlotId, int32 Generation, FName DriverOwner,
	int32 SourceLifeGeneration, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindLeaseIndex(SlotId);
	if (!Leases.IsValidIndex(Index) || Leases[Index].Generation != Generation
		|| Leases[Index].SourceLifeGeneration != SourceLifeGeneration || Leases[Index].DriverOwner != DriverOwner
		|| !Leases[Index].bPending)
	{
		OutError = TEXT("LeaseCommitRejected");
		return false;
	}
	Leases[Index].bPending = false;
	Leases[Index].bLive = true;
	return true;
}

bool UAuraEncounterCoordinator::IsDriverOwner(FName SlotId, int32 Generation, FName DriverOwner,
	int32 SourceLifeGeneration) const
{
	const int32 Index = FindLeaseIndex(SlotId);
	return Leases.IsValidIndex(Index) && Leases[Index].Generation == Generation
		&& Leases[Index].SourceLifeGeneration == SourceLifeGeneration && Leases[Index].DriverOwner == DriverOwner;
}

bool UAuraEncounterCoordinator::ReleaseSlot(FName SlotId, int32 Generation, FName DriverOwner,
	int32 SourceLifeGeneration, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindLeaseIndex(SlotId);
	if (!Leases.IsValidIndex(Index) || Leases[Index].Generation != Generation
		|| Leases[Index].SourceLifeGeneration != SourceLifeGeneration || Leases[Index].DriverOwner != DriverOwner)
	{
		OutError = TEXT("LeaseReleaseRejected");
		return false;
	}
	Leases.RemoveAt(Index);
	return true;
}

bool UAuraEncounterCoordinator::RecordAcceptedDeath(FName EncounterId, FName SlotId, int32 Generation,
	int32 SourceLifeGeneration, int32 DeathSequence, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindLeaseIndex(SlotId);
	if (!Leases.IsValidIndex(Index) || Leases[Index].EncounterId != EncounterId || Leases[Index].Generation != Generation
		|| Leases[Index].SourceLifeGeneration != SourceLifeGeneration || !Leases[Index].bLive
		|| Leases[Index].bDeathRecorded || SourceLifeGeneration <= 0 || DeathSequence <= 0)
	{
		OutError = TEXT("DeathLeaseRejected");
		return false;
	}
	const FString Key = FString::Printf(TEXT("%s|%s|%s|%d|%d|%d|%d"), *RunId.ToString(), *EncounterId.ToString(),
		*SlotId.ToString(), Epoch, Generation, SourceLifeGeneration, DeathSequence);
	if (AcceptedDeathKeys.Contains(Key))
	{
		OutError = TEXT("DuplicateDeath");
		return false;
	}
	AcceptedDeathKeys.Add(Key);
	Leases[Index].bDeathRecorded = true;
	Leases[Index].bLive = false;
	Leases[Index].bPending = false;
	++AcceptedDeathCount;
	return true;
}

FString UAuraEncounterCoordinator::MakeHeavyIdentityKey(const FGuid& InRunId, int32 InEpoch, FName EncounterId,
	int32 EncounterGeneration, FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId,
	int32 AttackSequence)
{
	return FString::Printf(TEXT("%s|%d|%s|%d|%s|%s|%d|%s|%d"), *InRunId.ToString(), InEpoch, *EncounterId.ToString(),
		EncounterGeneration, *DriverOwner.ToString(), *SourceEntityId.ToString(), SourceLifeGeneration, *AttackId.ToString(),
		AttackSequence);
}

FString UAuraEncounterCoordinator::MakeSupportIdentityKey(const FGuid& InRunId, int32 InEpoch, FName EncounterId,
	int32 EncounterGeneration, FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId)
{
	return FString::Printf(TEXT("%s|%d|%s|%d|%s|%s|%d|%s"), *InRunId.ToString(), InEpoch, *EncounterId.ToString(),
		EncounterGeneration, *DriverOwner.ToString(), *SourceEntityId.ToString(), SourceLifeGeneration, *FieldId.ToString());
}

FString UAuraEncounterCoordinator::MakeParticipantFingerprint(const TArray<FName>& Participants,
	TArray<FName>* OutCanonicalParticipants)
{
	TArray<FString> Names;
	for (const FName Participant : Participants) Names.Add(Participant.ToString());
	Names.Sort();
	if (OutCanonicalParticipants)
	{
		OutCanonicalParticipants->Reset();
		for (const FString& Name : Names) OutCanonicalParticipants->Add(FName(*Name));
	}
	return FString::Join(Names, TEXT(","));
}

FString UAuraEncounterCoordinator::MakeHeavyLeaseFingerprint(const FAuraHeavyAttackLease& Lease)
{
	return FString::Printf(TEXT("%.9f|%s"), Lease.ExpiryServerTime,
		*MakeParticipantFingerprint(Lease.AffectedParticipants));
}

FString UAuraEncounterCoordinator::MakeSupportLeaseFingerprint(const FAuraSupportFieldLease& Lease)
{
	return FString::Printf(TEXT("%.9f|%d|%s"), Lease.ChannelEndServerTime, Lease.MaxAffectedParticipants,
		*MakeParticipantFingerprint(Lease.AffectedParticipants));
}

bool UAuraEncounterCoordinator::HasParticipant(const FAuraHeavyAttackLease& Lease, FName ParticipantId)
{
	return !ParticipantId.IsNone() && Lease.AffectedParticipants.Contains(ParticipantId);
}

void UAuraEncounterCoordinator::RememberHeavyRequest(const FString& IdentityKey, const FString& Fingerprint,
	int32 LeaseSerial)
{
	if (!HeavyRequestFingerprints.Contains(IdentityKey)) HeavyRequestHistory.Add(IdentityKey);
	HeavyRequestFingerprints.Add(IdentityKey, Fingerprint);
	HeavyRequestSerials.Add(IdentityKey, LeaseSerial);
	while (HeavyRequestHistory.Num() > AuraEncounterCoordinatorPrivate::MaxRequestHistory)
	{
		const FString OldestKey = HeavyRequestHistory[0];
		HeavyRequestHistory.RemoveAt(0);
		HeavyRequestFingerprints.Remove(OldestKey);
		HeavyRequestSerials.Remove(OldestKey);
	}
}

void UAuraEncounterCoordinator::RememberSupportRequest(const FString& IdentityKey, const FString& Fingerprint,
	int32 LeaseSerial)
{
	if (!SupportRequestFingerprints.Contains(IdentityKey)) SupportRequestHistory.Add(IdentityKey);
	SupportRequestFingerprints.Add(IdentityKey, Fingerprint);
	SupportRequestSerials.Add(IdentityKey, LeaseSerial);
	while (SupportRequestHistory.Num() > AuraEncounterCoordinatorPrivate::MaxRequestHistory)
	{
		const FString OldestKey = SupportRequestHistory[0];
		SupportRequestHistory.RemoveAt(0);
		SupportRequestFingerprints.Remove(OldestKey);
		SupportRequestSerials.Remove(OldestKey);
	}
}

void UAuraEncounterCoordinator::CompleteHeavyLeaseAt(int32 Index)
{
	if (HeavyAttackLeases.IsValidIndex(Index)) HeavyAttackLeases.RemoveAt(Index);
}

void UAuraEncounterCoordinator::CompleteSupportFieldLeaseAt(int32 Index)
{
	if (SupportFieldLeases.IsValidIndex(Index)) SupportFieldLeases.RemoveAt(Index);
}

EAuraEncounterAdmissionResult UAuraEncounterCoordinator::TryAcquireHeavyAttackLease(FName EncounterId,
	int32 EncounterGeneration, FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId,
	int32 AttackSequence, const TArray<FName>& AffectedParticipants, int32 MaxConcurrentHeavyAttacks, int32 MaxTokensPerParticipant,
	double Now, double ExpiryServerTime, int32& OutLeaseSerial, FString& OutError)
{
	OutError.Reset();
	OutLeaseSerial = 0;
	if (!RunId.IsValid() || Epoch <= 0 || EncounterId.IsNone() || EncounterGeneration <= 0 || DriverOwner.IsNone()
		|| SourceEntityId.IsNone() || SourceLifeGeneration <= 0 || AttackId.IsNone() || AttackSequence <= 0
		|| MaxConcurrentHeavyAttacks <= 0
		|| MaxTokensPerParticipant <= 0 || !FMath::IsFinite(Now) || !FMath::IsFinite(ExpiryServerTime)
		|| ExpiryServerTime <= Now)
	{
		OutError = TEXT("HeavyAttackIdentityInvalid");
		return EAuraEncounterAdmissionResult::Rejected;
	}
	TArray<FName> CanonicalParticipants;
	if (!AuraEncounterCoordinatorPrivate::ValidateParticipants(AffectedParticipants, MAX_int32, CanonicalParticipants, OutError))
	{
		return EAuraEncounterAdmissionResult::Rejected;
	}
	const FString IdentityKey = MakeHeavyIdentityKey(RunId, Epoch, EncounterId, EncounterGeneration, DriverOwner,
		SourceEntityId, SourceLifeGeneration, AttackId, AttackSequence);
	const FString Fingerprint = FString::Printf(TEXT("%.9f|%s"), ExpiryServerTime,
		*MakeParticipantFingerprint(CanonicalParticipants));
	PruneExpiredLeases(Now);
	if (const FString* ExistingFingerprint = HeavyRequestFingerprints.Find(IdentityKey))
	{
		if (*ExistingFingerprint == Fingerprint)
		{
			OutLeaseSerial = HeavyRequestSerials.FindRef(IdentityKey);
			OutError = TEXT("DuplicateHeavyAttack");
			return EAuraEncounterAdmissionResult::DuplicateAccepted;
		}
		OutError = TEXT("HeavyAttackIdentityConflict");
		return EAuraEncounterAdmissionResult::Conflict;
	}
	const int32 ActiveIndex = FindActiveHeavyLeaseIndex(IdentityKey);
	if (HeavyAttackLeases.IsValidIndex(ActiveIndex))
	{
		const FAuraHeavyAttackLease& ActiveLease = HeavyAttackLeases[ActiveIndex];
		if (MakeHeavyLeaseFingerprint(ActiveLease) == Fingerprint)
		{
			OutLeaseSerial = ActiveLease.LeaseSerial;
			RememberHeavyRequest(IdentityKey, Fingerprint, ActiveLease.LeaseSerial);
			OutError = TEXT("DuplicateHeavyAttack");
			return EAuraEncounterAdmissionResult::DuplicateAccepted;
		}
		OutError = TEXT("HeavyAttackIdentityConflict");
		return EAuraEncounterAdmissionResult::Conflict;
	}
	if (HeavyAttackLeases.Num() >= MaxConcurrentHeavyAttacks)
	{
		OutError = TEXT("HeavyAttackCapacityFull");
		return EAuraEncounterAdmissionResult::CapacityFull;
	}
	for (const FName ParticipantId : CanonicalParticipants)
	{
		if (GetHeavyTokenCountForParticipant(ParticipantId) >= MaxTokensPerParticipant)
		{
			OutError = TEXT("ParticipantHeavyTokenCapacityFull");
			return EAuraEncounterAdmissionResult::CapacityFull;
		}
	}
	if (NextHeavyLeaseSerial <= 0 || NextHeavyLeaseSerial == MAX_int32)
	{
		OutError = TEXT("HeavyLeaseSerialExhausted");
		return EAuraEncounterAdmissionResult::Rejected;
	}
	FAuraHeavyAttackLease& Lease = HeavyAttackLeases.AddDefaulted_GetRef();
	Lease.RunId = RunId;
	Lease.Epoch = Epoch;
	Lease.EncounterId = EncounterId;
	Lease.EncounterGeneration = EncounterGeneration;
	Lease.DriverOwner = DriverOwner;
	Lease.SourceEntityId = SourceEntityId;
	Lease.SourceLifeGeneration = SourceLifeGeneration;
	Lease.AttackId = AttackId;
	Lease.AttackSequence = AttackSequence;
	Lease.LeaseSerial = NextHeavyLeaseSerial++;
	Lease.ExpiryServerTime = ExpiryServerTime;
	Lease.AffectedParticipants = MoveTemp(CanonicalParticipants);
	OutLeaseSerial = Lease.LeaseSerial;
	RememberHeavyRequest(IdentityKey, Fingerprint, Lease.LeaseSerial);
	return EAuraEncounterAdmissionResult::Acquired;
}

bool UAuraEncounterCoordinator::ReleaseHeavyAttackLease(FName EncounterId, int32 EncounterGeneration,
	FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId, int32 AttackSequence,
	int32 LeaseSerial,
	FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindHeavyLeaseIndex(LeaseSerial);
	if (!HeavyAttackLeases.IsValidIndex(Index))
	{
		OutError = TEXT("HeavyLeaseReleaseRejected");
		return false;
	}
	const FAuraHeavyAttackLease& Lease = HeavyAttackLeases[Index];
	if (Lease.RunId != RunId || Lease.Epoch != Epoch || Lease.EncounterId != EncounterId
		|| Lease.EncounterGeneration != EncounterGeneration || Lease.DriverOwner != DriverOwner
		|| Lease.SourceEntityId != SourceEntityId || Lease.SourceLifeGeneration != SourceLifeGeneration
		|| Lease.AttackId != AttackId || Lease.AttackSequence != AttackSequence)
	{
		OutError = TEXT("HeavyLeaseOwnerRejected");
		return false;
	}
	CompleteHeavyLeaseAt(Index);
	return true;
}

EAuraEncounterAdmissionResult UAuraEncounterCoordinator::TryAcquireSupportFieldLease(FName EncounterId,
	int32 EncounterGeneration, FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId,
	const TArray<FName>& AffectedParticipants, int32 MaxAffectedParticipants, double Now,
	double ChannelEndServerTime, int32& OutLeaseSerial, FString& OutError)
{
	OutError.Reset();
	OutLeaseSerial = 0;
	if (!RunId.IsValid() || Epoch <= 0 || EncounterId.IsNone() || EncounterGeneration <= 0 || DriverOwner.IsNone()
		|| SourceEntityId.IsNone() || SourceLifeGeneration <= 0 || FieldId.IsNone() || !FMath::IsFinite(Now)
		|| !FMath::IsFinite(ChannelEndServerTime) || ChannelEndServerTime <= Now)
	{
		OutError = TEXT("SupportFieldIdentityInvalid");
		return EAuraEncounterAdmissionResult::Rejected;
	}
	TArray<FName> CanonicalParticipants;
	if (!AuraEncounterCoordinatorPrivate::ValidateParticipants(AffectedParticipants, MaxAffectedParticipants,
		CanonicalParticipants, OutError))
	{
		return EAuraEncounterAdmissionResult::Rejected;
	}
	const FString IdentityKey = MakeSupportIdentityKey(RunId, Epoch, EncounterId, EncounterGeneration, DriverOwner,
		SourceEntityId, SourceLifeGeneration, FieldId);
	const FString Fingerprint = FString::Printf(TEXT("%.9f|%d|%s"), ChannelEndServerTime, MaxAffectedParticipants,
		*MakeParticipantFingerprint(CanonicalParticipants));
	PruneExpiredLeases(Now);
	if (const FString* ExistingFingerprint = SupportRequestFingerprints.Find(IdentityKey))
	{
		if (*ExistingFingerprint == Fingerprint)
		{
			OutLeaseSerial = SupportRequestSerials.FindRef(IdentityKey);
			OutError = TEXT("DuplicateSupportField");
			return EAuraEncounterAdmissionResult::DuplicateAccepted;
		}
		OutError = TEXT("SupportFieldIdentityConflict");
		return EAuraEncounterAdmissionResult::Conflict;
	}
	const int32 ActiveIndex = FindActiveSupportFieldLeaseIndex(IdentityKey);
	if (SupportFieldLeases.IsValidIndex(ActiveIndex))
	{
		const FAuraSupportFieldLease& ActiveLease = SupportFieldLeases[ActiveIndex];
		if (MakeSupportLeaseFingerprint(ActiveLease) == Fingerprint)
		{
			OutLeaseSerial = ActiveLease.LeaseSerial;
			RememberSupportRequest(IdentityKey, Fingerprint, ActiveLease.LeaseSerial);
			OutError = TEXT("DuplicateSupportField");
			return EAuraEncounterAdmissionResult::DuplicateAccepted;
		}
		OutError = TEXT("SupportFieldIdentityConflict");
		return EAuraEncounterAdmissionResult::Conflict;
	}
	if (NextSupportFieldLeaseSerial <= 0 || NextSupportFieldLeaseSerial == MAX_int32)
	{
		OutError = TEXT("SupportLeaseSerialExhausted");
		return EAuraEncounterAdmissionResult::Rejected;
	}
	FAuraSupportFieldLease& Lease = SupportFieldLeases.AddDefaulted_GetRef();
	Lease.RunId = RunId;
	Lease.Epoch = Epoch;
	Lease.EncounterId = EncounterId;
	Lease.EncounterGeneration = EncounterGeneration;
	Lease.DriverOwner = DriverOwner;
	Lease.SourceEntityId = SourceEntityId;
	Lease.SourceLifeGeneration = SourceLifeGeneration;
	Lease.FieldId = FieldId;
	Lease.LeaseSerial = NextSupportFieldLeaseSerial++;
	Lease.ChannelEndServerTime = ChannelEndServerTime;
	Lease.MaxAffectedParticipants = MaxAffectedParticipants;
	Lease.AffectedParticipants = MoveTemp(CanonicalParticipants);
	OutLeaseSerial = Lease.LeaseSerial;
	RememberSupportRequest(IdentityKey, Fingerprint, Lease.LeaseSerial);
	return EAuraEncounterAdmissionResult::Acquired;
}

bool UAuraEncounterCoordinator::UpdateSupportFieldCoverage(FName EncounterId, int32 EncounterGeneration,
	FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId, int32 LeaseSerial,
	const TArray<FName>& AffectedParticipants, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindSupportFieldLeaseIndex(LeaseSerial);
	if (!SupportFieldLeases.IsValidIndex(Index))
	{
		OutError = TEXT("SupportFieldUpdateRejected");
		return false;
	}
	FAuraSupportFieldLease& Lease = SupportFieldLeases[Index];
	if (Lease.RunId != RunId || Lease.Epoch != Epoch || Lease.EncounterId != EncounterId
		|| Lease.EncounterGeneration != EncounterGeneration || Lease.DriverOwner != DriverOwner
		|| Lease.SourceEntityId != SourceEntityId || Lease.SourceLifeGeneration != SourceLifeGeneration
		|| Lease.FieldId != FieldId)
	{
		OutError = TEXT("SupportFieldOwnerRejected");
		return false;
	}
	TArray<FName> CanonicalParticipants;
	if (!AuraEncounterCoordinatorPrivate::ValidateParticipants(AffectedParticipants, Lease.MaxAffectedParticipants,
		CanonicalParticipants, OutError))
	{
		return false;
	}
	Lease.AffectedParticipants = MoveTemp(CanonicalParticipants);
	return true;
}

bool UAuraEncounterCoordinator::ReleaseSupportFieldLease(FName EncounterId, int32 EncounterGeneration,
	FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId, int32 LeaseSerial,
	EAuraSupportFieldTerminationReason Reason, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindSupportFieldLeaseIndex(LeaseSerial);
	if (!SupportFieldLeases.IsValidIndex(Index) || SourceLifeGeneration <= 0
		|| static_cast<uint8>(Reason) > static_cast<uint8>(EAuraSupportFieldTerminationReason::Replaced))
	{
		OutError = TEXT("SupportFieldReleaseRejected");
		return false;
	}
	const FAuraSupportFieldLease& Lease = SupportFieldLeases[Index];
	if (Lease.RunId != RunId || Lease.Epoch != Epoch || Lease.EncounterId != EncounterId
		|| Lease.EncounterGeneration != EncounterGeneration || Lease.DriverOwner != DriverOwner
		|| Lease.SourceEntityId != SourceEntityId || Lease.SourceLifeGeneration != SourceLifeGeneration
		|| Lease.FieldId != FieldId)
	{
		OutError = TEXT("SupportFieldOwnerRejected");
		return false;
	}
	CompleteSupportFieldLeaseAt(Index);
	return true;
}

int32 UAuraEncounterCoordinator::PruneExpiredLeases(double Now)
{
	if (!FMath::IsFinite(Now)) return 0;
	int32 Removed = 0;
	for (int32 Index = HeavyAttackLeases.Num() - 1; Index >= 0; --Index)
	{
		if (HeavyAttackLeases[Index].ExpiryServerTime <= Now)
		{
			CompleteHeavyLeaseAt(Index);
			++Removed;
		}
	}
	for (int32 Index = SupportFieldLeases.Num() - 1; Index >= 0; --Index)
	{
		if (SupportFieldLeases[Index].ChannelEndServerTime <= Now)
		{
			CompleteSupportFieldLeaseAt(Index);
			++Removed;
		}
	}
	return Removed;
}

int32 UAuraEncounterCoordinator::InvalidateEncounterGeneration(FName EncounterId, int32 EncounterGeneration,
	FName DriverOwner, FString& OutError)
{
	OutError.Reset();
	if (EncounterId.IsNone() || EncounterGeneration <= 0 || DriverOwner.IsNone())
	{
		OutError = TEXT("EncounterGenerationInvalid");
		return 0;
	}
	int32 Removed = 0;
	for (int32 Index = HeavyAttackLeases.Num() - 1; Index >= 0; --Index)
	{
		const FAuraHeavyAttackLease& Lease = HeavyAttackLeases[Index];
		if (Lease.EncounterId == EncounterId && Lease.EncounterGeneration == EncounterGeneration
			&& Lease.DriverOwner == DriverOwner)
		{
			CompleteHeavyLeaseAt(Index);
			++Removed;
		}
	}
	for (int32 Index = SupportFieldLeases.Num() - 1; Index >= 0; --Index)
	{
		const FAuraSupportFieldLease& Lease = SupportFieldLeases[Index];
		if (Lease.EncounterId == EncounterId && Lease.EncounterGeneration == EncounterGeneration
			&& Lease.DriverOwner == DriverOwner)
		{
			CompleteSupportFieldLeaseAt(Index);
			++Removed;
		}
	}
	return Removed;
}

int32 UAuraEncounterCoordinator::GetHeavyTokenCountForParticipant(FName ParticipantId) const
{
	if (ParticipantId.IsNone()) return 0;
	int32 Count = 0;
	for (const FAuraHeavyAttackLease& Lease : HeavyAttackLeases)
	{
		if (HasParticipant(Lease, ParticipantId)) ++Count;
	}
	return Count;
}

int32 UAuraEncounterCoordinator::GetEffectiveSupportFieldLeaseSerial(FName EncounterId,
	int32 EncounterGeneration, FName ParticipantId) const
{
	if (EncounterId.IsNone() || EncounterGeneration <= 0 || ParticipantId.IsNone()) return 0;
	int32 EffectiveSerial = 0;
	for (const FAuraSupportFieldLease& Lease : SupportFieldLeases)
	{
		if (Lease.EncounterId != EncounterId || Lease.EncounterGeneration != EncounterGeneration
			|| !Lease.AffectedParticipants.Contains(ParticipantId))
		{
			continue;
		}
		if (EffectiveSerial == 0 || Lease.LeaseSerial < EffectiveSerial) EffectiveSerial = Lease.LeaseSerial;
	}
	return EffectiveSerial;
}

void UAuraEncounterCoordinator::Reset()
{
	RunId.Invalidate();
	Epoch = 0;
	MaxLiveHostiles = 0;
	AcceptedDeathCount = 0;
	Leases.Reset();
	AcceptedDeathKeys.Reset();
	HeavyAttackLeases.Reset();
	SupportFieldLeases.Reset();
	NextHeavyLeaseSerial = 1;
	NextSupportFieldLeaseSerial = 1;
	HeavyRequestFingerprints.Reset();
	HeavyRequestSerials.Reset();
	HeavyRequestHistory.Reset();
	SupportRequestFingerprints.Reset();
	SupportRequestSerials.Reset();
	SupportRequestHistory.Reset();
}
