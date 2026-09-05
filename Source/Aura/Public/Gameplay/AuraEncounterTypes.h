// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraEncounterTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraEncounterAdmissionResult : uint8
{
	Rejected,
	Acquired,
	DuplicateAccepted,
	CapacityFull,
	Conflict
};

UENUM(BlueprintType)
enum class EAuraSupportFieldTerminationReason : uint8
{
	Expired,
	OwnerDeath,
	RangeDeparture,
	Cancelled,
	EncounterInvalidated,
	Replaced
};

USTRUCT(BlueprintType)
struct AURA_API FAuraEncounterSlotLease
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	FName SlotId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	int32 Generation = 0;

	/** Source actor life/spawn generation; it changes when a logical slot is replaced. */
	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	int32 SourceLifeGeneration = 1;

	/** Immutable logical owner of movement/attack behavior for this slot generation. */
	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	FName DriverOwner = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	bool bPending = true;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	bool bLive = false;

	/** Terminal receipt guard; a slot can contribute at most one accepted death. */
	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter")
	bool bDeathRecorded = false;
};

/** Encounter-owned admission lease for one committed heavy attack. */
USTRUCT(BlueprintType)
struct AURA_API FAuraHeavyAttackLease
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	int32 EncounterGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	FName DriverOwner = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	FName SourceEntityId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	int32 SourceLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	FName AttackId = NAME_None;

	/** Authority-issued attack instance identity; the definition ID alone is reusable. */
	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	int32 AttackSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	int32 LeaseSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	double ExpiryServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|HeavyAttack")
	TArray<FName> AffectedParticipants;
};

/** Source-owned support channel lease; effective ownership is derived by serial, never by actor pointer. */
USTRUCT(BlueprintType)
struct AURA_API FAuraSupportFieldLease
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	int32 EncounterGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	FName DriverOwner = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	FName SourceEntityId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	int32 SourceLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	FName FieldId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	int32 LeaseSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	double ChannelEndServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	int32 MaxAffectedParticipants = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Encounter|Support")
	TArray<FName> AffectedParticipants;
};
