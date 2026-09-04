// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraMissionTypes.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EAuraMissionPhase : uint8
{
	Hub,
	Preparing,
	Active,
	Extraction,
	Settling,
	Completed,
	Failed,
	Aborted
};

UENUM(BlueprintType)
enum class EAuraMissionMemberState : uint8
{
	Connected,
	Disconnected,
	Incapacitated,
	Forfeited
};

UENUM(BlueprintType)
enum class EAuraMissionMutationResult : uint8
{
	Rejected,
	Accepted,
	CellCompleted,
	ObjectiveCompleted
};

USTRUCT(BlueprintType)
struct AURA_API FAuraMissionDeathKey
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName LeaseId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Generation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 DeathSequence = 0;

	bool operator==(const FAuraMissionDeathKey& Other) const
	{
		return LeaseId == Other.LeaseId && Epoch == Other.Epoch && Generation == Other.Generation
			&& DeathSequence == Other.DeathSequence;
	}
};

USTRUCT(BlueprintType)
struct AURA_API FAuraMissionMemberSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Member")
	FName RoleId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Member")
	EAuraMissionMemberState State = EAuraMissionMemberState::Connected;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Member")
	bool bConnected = true;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Member")
	bool bEligibleForSettlement = true;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraMissionCellState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Cell")
	FName CellId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Cell")
	int32 RequiredDeaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Cell")
	int32 AcceptedDeaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission|Cell")
	bool bCompleted = false;

	/** Authority-owned source/encounter generation currently allowed to report deaths for this cell. */
	UPROPERTY(BlueprintReadOnly, Category = "Mission|Cell")
	int32 ActiveGeneration = 1;

	/** Authority-only dedupe ledger. It is never copied into the public mission actor. */
	UPROPERTY(Transient)
	TArray<FAuraMissionDeathKey> AcceptedDeathKeys;

	bool HasAcceptedDeath(const FAuraMissionDeathKey& Key) const
	{
		return AcceptedDeathKeys.Contains(Key);
	}
};

/** Client-visible mission state. Authority-only cell ledgers and death receipts stay out of replication. */
USTRUCT(BlueprintType)
struct AURA_API FAuraMissionPublicSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName GameplayProfile = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName ArrangementId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	EAuraMissionPhase Phase = EAuraMissionPhase::Hub;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	double PhaseDeadlineServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	double RunDeadlineServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 CurrentCellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName CurrentCellId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 CurrentCellAcceptedDeaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 CurrentCellRequiredDeaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 CompletedCellCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bObjectiveComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName TerminalReason = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TArray<FAuraMissionMemberSnapshot> Members;
};

/**
 * Authoritative, serializable mission lifecycle state. The world subsystem owns
 * mutation; keeping transition rules here makes duplicate callbacks and stale
 * encounter events reject consistently in native tests and runtime code.
 */
USTRUCT(BlueprintType)
struct AURA_API FAuraMissionRunState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName GameplayProfile = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName MissionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName ArrangementId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	EAuraMissionPhase Phase = EAuraMissionPhase::Hub;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	double PhaseDeadlineServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	double RunDeadlineServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	int32 CurrentCellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	bool bObjectiveComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	FName TerminalReason = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Mission")
	TArray<FAuraMissionMemberSnapshot> Members;

	/** Authority-owned cell progress; public state receives only the derived counters. */
	UPROPERTY(Transient)
	TArray<FAuraMissionCellState> Cells;

	bool IsTerminal() const
	{
		return Phase == EAuraMissionPhase::Completed || Phase == EAuraMissionPhase::Failed || Phase == EAuraMissionPhase::Aborted;
	}

	bool IsActiveOrExtracting() const
	{
		return Phase == EAuraMissionPhase::Active || Phase == EAuraMissionPhase::Extraction;
	}

	bool TryBeginPreparation(const FGuid& InRunId, int32 InEpoch, FName InProfile, FName InMissionId,
		FName InArrangementId, double Now, double RunCapSeconds, FString& OutError);
	bool ConfigureClearCells(const TArray<FAuraMissionCellState>& InCells, FString& OutError);
	bool TryAdvanceCurrentCellGeneration(int32 NewGeneration, FString& OutError);
	bool TryActivate(double Now, FString& OutError);
	bool TryBeginExtraction(double Now, double ExtractionSeconds, FString& OutError);
	bool TryBeginSettlement(bool bSuccess, FString& OutError);
	bool TryCommitSettlement(FString& OutError);
	bool TryCancelPreparation(FName Reason, FString& OutError);
	bool TryCancelPreparationAt(double Now, FName Reason, FString& OutError);
	bool TryFail(FName Reason, FString& OutError);
	bool TryAbort(FName Reason, FString& OutError);
	EAuraMissionMutationResult RegisterCellDeath(FName CellId, FName LeaseId, int32 Generation,
		int32 DeathSequence, FString& OutError);
	bool TryRegisterCellDeath(FName CellId, FName LeaseId, int32 Generation, int32 DeathSequence, FString& OutError);

	FAuraMissionPublicSnapshot BuildPublicSnapshot() const;

	FName GetCurrentCellId() const
	{
		return Cells.IsValidIndex(CurrentCellIndex) ? Cells[CurrentCellIndex].CellId : NAME_None;
	}

private:
	bool Touch(FString& OutError);
	bool IsValidTransition(EAuraMissionPhase From, EAuraMissionPhase To) const;
};
