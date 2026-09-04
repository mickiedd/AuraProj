// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Gameplay/AuraEncounterTypes.h"
#include "AuraEncounterCoordinator.generated.h"

/** Authority-only bounded lease ledger for mission-managed enemies. */
UCLASS(Transient)
class AURA_API UAuraEncounterCoordinator : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(const FGuid& InRunId, int32 InEpoch, int32 InMaxLiveHostiles);
	bool TryReserveSlot(FName EncounterId, FName SlotId, int32 Generation, FName DriverOwner,
		int32 SourceLifeGeneration, FString& OutError);
	bool CommitSlot(FName SlotId, int32 Generation, FName DriverOwner, int32 SourceLifeGeneration, FString& OutError);
	bool ReleaseSlot(FName SlotId, int32 Generation, FName DriverOwner, int32 SourceLifeGeneration, FString& OutError);
	bool RecordAcceptedDeath(FName EncounterId, FName SlotId, int32 Generation, int32 SourceLifeGeneration,
		int32 DeathSequence, FString& OutError);

	/**
	 * Atomically admits every participant token required by a committed heavy
	 * attack. A duplicate identity is idempotent while a changed payload is a
	 * conflict; neither path admits a second lease.
	 */
	EAuraEncounterAdmissionResult TryAcquireHeavyAttackLease(FName EncounterId, int32 EncounterGeneration,
		FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId,
		int32 AttackSequence, const TArray<FName>& AffectedParticipants, int32 MaxConcurrentHeavyAttacks,
		int32 MaxTokensPerParticipant, double Now, double ExpiryServerTime, int32& OutLeaseSerial, FString& OutError);
	bool ReleaseHeavyAttackLease(FName EncounterId, int32 EncounterGeneration, FName DriverOwner,
		FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId, int32 AttackSequence, int32 LeaseSerial,
		FString& OutError);

	/** Registers one source-owned support channel; overlapping ownership is derived by lowest live serial. */
	EAuraEncounterAdmissionResult TryAcquireSupportFieldLease(FName EncounterId, int32 EncounterGeneration,
		FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId,
		const TArray<FName>& AffectedParticipants, int32 MaxAffectedParticipants, double Now,
		double ChannelEndServerTime, int32& OutLeaseSerial, FString& OutError);
	bool UpdateSupportFieldCoverage(FName EncounterId, int32 EncounterGeneration, FName DriverOwner,
		FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId, int32 LeaseSerial,
		const TArray<FName>& AffectedParticipants, FString& OutError);
	bool ReleaseSupportFieldLease(FName EncounterId, int32 EncounterGeneration, FName DriverOwner,
		FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId, int32 LeaseSerial,
		EAuraSupportFieldTerminationReason Reason, FString& OutError);

	/** Removes expired leases and returns the number of terminal leases removed. */
	int32 PruneExpiredLeases(double Now);
	int32 InvalidateEncounterGeneration(FName EncounterId, int32 EncounterGeneration, FName DriverOwner,
		FString& OutError);
	void Reset();

	const FGuid& GetRunId() const { return RunId; }
	int32 GetEpoch() const { return Epoch; }
	int32 GetMaxLiveHostiles() const { return MaxLiveHostiles; }
	int32 GetPendingCount() const;
	int32 GetLiveCount() const;
	int32 GetAcceptedDeathCount() const { return AcceptedDeathCount; }
	const TArray<FAuraEncounterSlotLease>& GetLeases() const { return Leases; }
	bool IsDriverOwner(FName SlotId, int32 Generation, FName DriverOwner, int32 SourceLifeGeneration) const;
	int32 GetHeavyAttackLeaseCount() const { return HeavyAttackLeases.Num(); }
	int32 GetSupportFieldLeaseCount() const { return SupportFieldLeases.Num(); }
	int32 GetHeavyTokenCountForParticipant(FName ParticipantId) const;
	int32 GetEffectiveSupportFieldLeaseSerial(FName EncounterId, int32 EncounterGeneration,
		FName ParticipantId) const;
	const TArray<FAuraHeavyAttackLease>& GetHeavyAttackLeases() const { return HeavyAttackLeases; }
	const TArray<FAuraSupportFieldLease>& GetSupportFieldLeases() const { return SupportFieldLeases; }

private:
	int32 FindLeaseIndex(FName SlotId) const;
	int32 FindHeavyLeaseIndex(int32 LeaseSerial) const;
	int32 FindActiveHeavyLeaseIndex(const FString& IdentityKey) const;
	int32 FindSupportFieldLeaseIndex(int32 LeaseSerial) const;
	int32 FindActiveSupportFieldLeaseIndex(const FString& IdentityKey) const;
	void RememberHeavyRequest(const FString& IdentityKey, const FString& Fingerprint, int32 LeaseSerial);
	void RememberSupportRequest(const FString& IdentityKey, const FString& Fingerprint, int32 LeaseSerial);
	void CompleteHeavyLeaseAt(int32 Index);
	void CompleteSupportFieldLeaseAt(int32 Index);
	static FString MakeHeavyIdentityKey(const FGuid& InRunId, int32 InEpoch, FName EncounterId,
		int32 EncounterGeneration, FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName AttackId,
		int32 AttackSequence);
	static FString MakeSupportIdentityKey(const FGuid& InRunId, int32 InEpoch, FName EncounterId,
		int32 EncounterGeneration, FName DriverOwner, FName SourceEntityId, int32 SourceLifeGeneration, FName FieldId);
	static FString MakeParticipantFingerprint(const TArray<FName>& Participants, TArray<FName>* OutCanonicalParticipants = nullptr);
	static FString MakeHeavyLeaseFingerprint(const FAuraHeavyAttackLease& Lease);
	static FString MakeSupportLeaseFingerprint(const FAuraSupportFieldLease& Lease);
	static bool HasParticipant(const FAuraHeavyAttackLease& Lease, FName ParticipantId);

	FGuid RunId;
	int32 Epoch = 0;
	int32 MaxLiveHostiles = 0;
	int32 AcceptedDeathCount = 0;
	TArray<FAuraEncounterSlotLease> Leases;
	TSet<FString> AcceptedDeathKeys;
	TArray<FAuraHeavyAttackLease> HeavyAttackLeases;
	TArray<FAuraSupportFieldLease> SupportFieldLeases;
	int32 NextHeavyLeaseSerial = 1;
	int32 NextSupportFieldLeaseSerial = 1;
	/** Bounded request replay caches; live lease arrays remain authoritative for active requests. */
	TMap<FString, FString> HeavyRequestFingerprints;
	TMap<FString, int32> HeavyRequestSerials;
	TArray<FString> HeavyRequestHistory;
	TMap<FString, FString> SupportRequestFingerprints;
	TMap<FString, int32> SupportRequestSerials;
	TArray<FString> SupportRequestHistory;
};
