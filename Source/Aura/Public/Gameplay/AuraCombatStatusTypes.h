// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraCombatStatusTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraCombatStatusResult : uint8
{
	Rejected,
	Applied,
	DuplicateAccepted,
	Removed
};

UENUM(BlueprintType)
enum class EAuraCombatStatusRemovalReason : uint8
{
	Expired,
	Interrupted,
	OwnerDeath,
	RangeDeparture,
	EncounterInvalidated,
	Replaced,
	Manual
};

UENUM(BlueprintType)
enum class EAuraInterruptResult : uint8
{
	Rejected,
	Interrupted,
	Immune,
	Duplicate
};

/** Generation-safe identity for a status application. */
USTRUCT(BlueprintType)
struct AURA_API FAuraCombatStatusKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 EncounterGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName SourceEntityId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 SourceLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName TargetEntityId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 TargetLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName StatusDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 ApplicationSequence = 0;

	bool IsValid() const
	{
		return RunId.IsValid() && Epoch > 0 && EncounterGeneration > 0 && !SourceEntityId.IsNone()
			&& SourceLifeGeneration > 0 && !TargetEntityId.IsNone() && TargetLifeGeneration > 0
			&& !StatusDefinitionId.IsNone() && ApplicationSequence > 0;
	}

	bool operator==(const FAuraCombatStatusKey& Other) const
	{
		return RunId == Other.RunId && Epoch == Other.Epoch && EncounterGeneration == Other.EncounterGeneration
			&& SourceEntityId == Other.SourceEntityId && SourceLifeGeneration == Other.SourceLifeGeneration
			&& TargetEntityId == Other.TargetEntityId && TargetLifeGeneration == Other.TargetLifeGeneration
			&& StatusDefinitionId == Other.StatusDefinitionId && ApplicationSequence == Other.ApplicationSequence;
	}
};

USTRUCT(BlueprintType)
struct AURA_API FAuraCombatStatusRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FAuraCombatStatusKey Key;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName StackingPolicy = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	double StartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	double ExpiryServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName InterruptClassification = NAME_None;
};

/** A minimal server-owned channel seam. The later GAS adapter consumes its terminal result. */
USTRUCT(BlueprintType)
struct AURA_API FAuraInterruptChannelKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 EncounterGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName TargetEntityId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 TargetLifeGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FName ChannelId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 ChannelGeneration = 0;

	bool IsValid() const
	{
		return RunId.IsValid() && Epoch > 0 && EncounterGeneration > 0 && !TargetEntityId.IsNone()
			&& TargetLifeGeneration > 0 && !ChannelId.IsNone() && ChannelGeneration > 0;
	}

	bool operator==(const FAuraInterruptChannelKey& Other) const
	{
		return RunId == Other.RunId && Epoch == Other.Epoch && EncounterGeneration == Other.EncounterGeneration
			&& TargetEntityId == Other.TargetEntityId && TargetLifeGeneration == Other.TargetLifeGeneration
			&& ChannelId == Other.ChannelId && ChannelGeneration == Other.ChannelGeneration;
	}
};

USTRUCT(BlueprintType)
struct AURA_API FAuraInterruptChannelState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	FAuraInterruptChannelKey Key;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	double StartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	double EndServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	double InterruptImmuneUntilServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	int32 LastInterruptSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Status")
	bool bActive = false;
};

/** Non-replicated authority ledger used by the status component. */
USTRUCT()
struct AURA_API FAuraCombatStatusLedger
{
	GENERATED_BODY()

	EAuraCombatStatusResult TryApplyStatus(const FAuraCombatStatusKey& Key, FName StackingPolicy,
		FName InterruptClassification, double Now, double DurationSeconds, FString& OutError);
	EAuraCombatStatusResult TryRemoveStatus(const FAuraCombatStatusKey& Key, EAuraCombatStatusRemovalReason Reason,
		FString& OutError);
	bool TryBeginInterruptibleChannel(const FAuraInterruptChannelKey& Key, double Now, double DurationSeconds,
		FString& OutError);
	EAuraInterruptResult TryInterruptChannel(const FAuraInterruptChannelKey& Key, int32 InterruptSequence,
		double Now, FString& OutError);
	int32 PruneExpired(double Now);

	const TArray<FAuraCombatStatusRecord>& GetStatuses() const { return Statuses; }
	const TArray<FAuraInterruptChannelState>& GetChannels() const { return Channels; }

private:
	int32 FindStatusIndex(const FAuraCombatStatusKey& Key) const;
	int32 FindChannelIndex(const FAuraInterruptChannelKey& Key) const;

	UPROPERTY(Transient)
	TArray<FAuraCombatStatusRecord> Statuses;

	UPROPERTY(Transient)
	TArray<FAuraInterruptChannelState> Channels;
};
