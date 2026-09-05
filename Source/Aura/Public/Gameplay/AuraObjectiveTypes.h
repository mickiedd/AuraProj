// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraObjectiveTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraObjectiveType : uint8
{
	Clear,
	Escort,
	InteractHold
};

UENUM(BlueprintType)
enum class EAuraObjectiveState : uint8
{
	Pending,
	Active,
	Completed,
	Failed,
	Cancelled
};

UENUM(BlueprintType)
enum class EAuraObjectiveMutationResult : uint8
{
	Rejected,
	Accepted,
	Completed,
	Duplicate,
	Busy,
	Cancelled
};

USTRUCT(BlueprintType)
struct AURA_API FAuraObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	EAuraObjectiveType Type = EAuraObjectiveType::Clear;

	/** Target identity is an encounter, shelter/destination, or relay. */
	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FName TargetId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	TArray<FName> PrerequisiteIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	TArray<FName> RequiredMemberIds;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 TargetCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	double DeadlineSeconds = 180.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	bool bRequired = true;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraObjectiveState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FAuraObjectiveDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	EAuraObjectiveState State = EAuraObjectiveState::Pending;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 ActiveGeneration = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 ProgressCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 TargetCount = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	double StartServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	double DeadlineServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FName LastFailureReason = NAME_None;

	UPROPERTY(Transient)
	FName ActiveChannelOwner = NAME_None;

	UPROPERTY(Transient)
	int32 ActiveChannelTargetRevision = 0;

	UPROPERTY(Transient)
	double ActiveChannelEndServerTime = 0.0;

	/** Authority-only dedupe ledger; it is excluded from the public snapshot. */
	UPROPERTY(Transient)
	TArray<FString> AcceptedEventKeys;

	UPROPERTY(Transient)
	TArray<FName> AcceptedEscortMemberIds;

	bool HasAcceptedEvent(const FString& Key) const { return AcceptedEventKeys.Contains(Key); }
};

USTRUCT(BlueprintType)
struct AURA_API FAuraObjectivePublicSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FName CurrentObjectiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	EAuraObjectiveState CurrentObjectiveState = EAuraObjectiveState::Pending;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 CurrentProgressCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 CurrentTargetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	double CurrentDeadlineServerTime = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 CompletedRequiredCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 RequiredObjectiveCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	bool bAllRequiredComplete = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	bool bExtractionOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FName LastFailureReason = NAME_None;
};

/**
 * Authority-only ordered objective reducer. It is intentionally not wired into
 * GameMode, input, AI, damage, persistence, or UI until the Day 40/41 entry
 * evidence and mission-space gates are available.
 */
USTRUCT(BlueprintType)
struct AURA_API FAuraObjectiveRunState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	FGuid RunId;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 Revision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	int32 CurrentObjectiveIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	bool bExtractionOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "Gameplay|Objective")
	TArray<FAuraObjectiveState> Objectives;

	bool Initialize(const FGuid& InRunId, int32 InEpoch);
	bool ConfigureObjectives(const TArray<FAuraObjectiveDefinition>& Definitions, FString& OutError);
	bool TryActivateNext(double Now, FString& OutError);

	EAuraObjectiveMutationResult RegisterClearDeath(const FGuid& EventRunId, int32 EventEpoch,
		FName ObjectiveId, int32 Generation, int32 DeathSequence, FString& OutError);
	EAuraObjectiveMutationResult RegisterEscortArrival(const FGuid& EventRunId, int32 EventEpoch,
		FName ObjectiveId, FName MemberId, FName DestinationId, int32 Generation, FString& OutError);

	EAuraObjectiveMutationResult TryBeginInteractHold(const FGuid& EventRunId, int32 EventEpoch,
		FName ObjectiveId, FName OwnerId, int32 TargetRevision, double Now, bool bAlive, float DistanceUnits,
		bool bLineOfSight, FString& OutError);
	EAuraObjectiveMutationResult TryCompleteInteractHold(const FGuid& EventRunId, int32 EventEpoch,
		FName ObjectiveId, FName OwnerId, int32 TargetRevision, double Now, bool bAlive, float DistanceUnits,
		bool bLineOfSight, FString& OutError);
	EAuraObjectiveMutationResult CancelInteractHold(FName ObjectiveId, FName OwnerId, FName Reason,
		FString& OutError);

	bool TryBeginExtraction(FString& OutError);
	bool AreRequiredObjectivesComplete() const;
	FAuraObjectivePublicSnapshot BuildPublicSnapshot() const;

private:
	bool Touch(FString& OutError);
	bool ValidateEventIdentity(const FGuid& EventRunId, int32 EventEpoch, FString& OutError) const;
	int32 FindObjectiveIndex(FName ObjectiveId) const;
	bool IsCurrentObjective(int32 Index, EAuraObjectiveType Type, FString& OutError) const;
	bool CompleteCurrentObjective(FString& OutError);
	bool FailCurrentObjective(FName Reason, FString& OutError);
};
