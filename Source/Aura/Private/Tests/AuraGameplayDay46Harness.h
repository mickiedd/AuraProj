// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

#include "Gameplay/AuraAttackTimelineTypes.h"
#include "Gameplay/AuraCombatStatusTypes.h"
#include "Gameplay/AuraMissionTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

class UAuraEncounterCoordinator;

/**
 * Stable, value-only projection of the real Days 42–45 authorities. It intentionally
 * excludes UObject addresses, hash iteration order, and any presentation/runtime data.
 */
struct FAuraDay46ContractSnapshot
{
	FGuid RunId;
	int32 Epoch = 0;
	FName EncounterId = NAME_None;
	FName DriverOwner = NAME_None;
	int32 EncounterGeneration = 0;

	EAuraMissionPhase MissionPhase = EAuraMissionPhase::Hub;
	int32 MissionRevision = 0;
	FName CurrentCellId = NAME_None;
	int32 CompletedCellCount = 0;
	bool bObjectiveComplete = false;

	EAuraAttackTimelinePhase AttackPhase = EAuraAttackTimelinePhase::Idle;
	FAuraAttackTimelineKey AttackKey;
	double AttackStartServerTime = 0.0;
	double AttackExpectedImpactServerTime = 0.0;
	double AttackDamageWindowEndServerTime = 0.0;
	double AttackRecoveryEndServerTime = 0.0;
	FName AttackTerminalReason = NAME_None;
	bool bAttackImpactResolved = false;

	TArray<FString> HeavyLeaseEntries;
	TArray<FString> HeavyTokenEntries;
	TArray<FString> SupportLeaseEntries;
	TArray<FString> SupportOwnerEntries;
	TArray<FString> StatusEntries;
	TArray<FString> ChannelEntries;

	bool operator==(const FAuraDay46ContractSnapshot& Other) const;
	bool operator!=(const FAuraDay46ContractSnapshot& Other) const { return !(*this == Other); }
};

/** Test-only deterministic driver that composes the existing Days 42–45 contracts. */
class FAuraDay46ContractHarness final
{
public:
	bool Initialize(const FGuid& InRunId, int32 InEpoch, FString& OutError);
	bool ExecutePrimaryScenario(FString& OutError);
	bool AdvanceEncounterGeneration(int32 NewGeneration, FString& OutError);

	FAuraAttackTimelineKey MakeAttackKey(FName SourceEntityId, int32 SourceLifeGeneration, int32 AttackSequence,
		int32 InEncounterGeneration = INDEX_NONE, FName InDriverOwner = NAME_None) const;
	FAuraAttackTimelineDefinition MakeAttackDefinition() const;
	FAuraCombatStatusKey MakeStatusKey(FName SourceEntityId, int32 SourceLifeGeneration, FName TargetEntityId,
		int32 TargetLifeGeneration, FName StatusDefinitionId, int32 ApplicationSequence,
		int32 InEncounterGeneration = INDEX_NONE) const;
	FAuraInterruptChannelKey MakeChannelKey(FName TargetEntityId, int32 TargetLifeGeneration, FName ChannelId,
		int32 ChannelGeneration, int32 InEncounterGeneration = INDEX_NONE) const;

	FAuraMissionRunState& GetMissionState() { return MissionState; }
	const FAuraMissionRunState& GetMissionState() const { return MissionState; }
	FAuraAttackTimelineState& GetAttackTimeline() { return AttackTimeline; }
	const FAuraAttackTimelineState& GetAttackTimeline() const { return AttackTimeline; }
	FAuraCombatStatusLedger& GetStatusLedger() { return StatusLedger; }
	const FAuraCombatStatusLedger& GetStatusLedger() const { return StatusLedger; }
	UAuraEncounterCoordinator& GetEncounterCoordinator() { return *EncounterCoordinator; }
	const UAuraEncounterCoordinator& GetEncounterCoordinator() const { return *EncounterCoordinator; }

	const FGuid& GetRunId() const { return RunId; }
	int32 GetEpoch() const { return Epoch; }
	FName GetEncounterId() const { return EncounterId; }
	FName GetDriverOwner() const { return DriverOwner; }
	int32 GetEncounterGeneration() const { return EncounterGeneration; }
	bool IsInitialized() const { return bInitialized && EncounterCoordinator != nullptr; }

	FAuraDay46ContractSnapshot BuildSnapshot() const;

private:
	static FString MakeAttackKeyEntry(const FAuraAttackTimelineKey& Key);
	static FString MakeStatusKeyEntry(const FAuraCombatStatusKey& Key);
	static FString MakeChannelKeyEntry(const FAuraInterruptChannelKey& Key);
	static FString MakeParticipantList(const TArray<FName>& Participants);

	bool bInitialized = false;
	FGuid RunId;
	int32 Epoch = 0;
	FName EncounterId = NAME_None;
	FName DriverOwner = NAME_None;
	int32 EncounterGeneration = 1;
	FAuraMissionRunState MissionState;
	FAuraAttackTimelineState AttackTimeline;
	FAuraCombatStatusLedger StatusLedger;
	UAuraEncounterCoordinator* EncounterCoordinator = nullptr;
};

#endif
