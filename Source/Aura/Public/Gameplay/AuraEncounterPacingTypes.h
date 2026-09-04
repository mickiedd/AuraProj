// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

enum class EAuraEncounterPacingPhase : uint8
{
	Build,
	Pressure,
	Recovery,
	Aborted
};

enum class EAuraPacingMutationResult : uint8
{
	Rejected,
	NoChange,
	PhaseChanged,
	AdmissionGranted,
	AdmissionDeferred,
	SlotTerminal,
	RosterComplete,
	TechnicalAbort
};

/** Value-only participant input observed by authority. Pacing never owns or mutates health. */
struct FAuraPacingParticipantState
{
	FName ParticipantId = NAME_None;
	bool bConnected = false;
	bool bAlive = false;
	bool bTargetableDormantProxy = false;
	float HealthNormalized = 0.0f;
};

/** One immutable member of the fixed required encounter roster. */
struct FAuraPacingRosterSlot
{
	FName SlotId = NAME_None;
	FName ArchetypeId = NAME_None;
	int32 Cost = 0;
	int32 Generation = 1;
	int32 SourceLifeGeneration = 1;
	bool bRequired = true;
	bool bAdmitted = false;
	bool bTerminal = false;
};

/** Deterministic, value-only projection used by native contract tests. */
struct FAuraEncounterPacingSnapshot
{
	FGuid RunId;
	int32 Epoch = 0;
	int32 Revision = 0;
	EAuraEncounterPacingPhase Phase = EAuraEncounterPacingPhase::Build;
	double PhaseStartServerTime = 0.0;
	double DeadlineServerTime = 0.0;
	double SmoothedIntensity = 0.0;
	int32 CurrentWaveCost = 0;
	int32 WaveAdmissionBudget = 0;
	int32 LiveHostileCount = 0;
	int32 PendingRequiredCount = 0;
	int32 ConnectedAliveCount = 0;
	int32 LiveDormantProxyCount = 0;
	int32 PhaseChangeCount = 0;
	bool bAdmissionPaused = true;
	bool bLastInputClamped = false;
	FName TerminalReason = NAME_None;
	TArray<FName> AdmissionHistory;

	bool operator==(const FAuraEncounterPacingSnapshot& Other) const;
	bool operator!=(const FAuraEncounterPacingSnapshot& Other) const { return !(*this == Other); }
};

/**
 * Inert Day 50 authority reducer. It owns pacing decisions only; encounter
 * leases, heavy tokens, actors, health, damage, deadlines, and HUD stay with
 * their existing systems until the runtime entry gates are satisfied.
 */
class AURA_API FAuraEncounterPacingState final
{
public:
	bool Initialize(bool bAuthority, const FGuid& InRunId, int32 InEpoch, int32 InParticipantCount,
		int32 InSeed, double InStartServerTime, double InDeadlineSeconds,
		const TArray<FAuraPacingRosterSlot>& InRequiredRoster, FString& OutError);

	EAuraPacingMutationResult Tick(bool bAuthority, double AuthorityNow, double RecentHealthLossFraction,
		double NearbyCommittedThreatFraction, const TArray<FAuraPacingParticipantState>& Participants,
		bool bChoiceOrSupplyInteractionOpen, FString& OutError);

	EAuraPacingMutationResult TryAdmitNext(bool bAuthority, double AuthorityNow, FName& OutSlotId,
		FString& OutError);

	EAuraPacingMutationResult MarkSlotTerminal(bool bAuthority, FName SlotId, int32 Generation,
		int32 SourceLifeGeneration, FString& OutError);

	FAuraEncounterPacingSnapshot BuildSnapshot() const;
	bool IsRequiredRosterComplete() const;
	const TArray<FAuraPacingRosterSlot>& GetRosterSlots() const { return RosterSlots; }

	static int32 CostForArchetype(FName ArchetypeId);

private:
	bool Touch(FString& OutError);
	void TransitionTo(EAuraEncounterPacingPhase NewPhase, double AuthorityNow);
	EAuraPacingMutationResult Abort(FName Reason, FString& OutError);
	int32 GetLiveHostileCount() const;
	int32 GetPendingRequiredCount() const;
	int32 FindSlotIndex(FName SlotId) const;

	FGuid RunId;
	int32 Epoch = 0;
	int32 Revision = 0;
	int32 ParticipantCount = 0;
	int32 Seed = 0;
	int32 WaveAdmissionBudget = 0;
	int32 ActiveHostileCap = 0;
	int32 CurrentWaveCost = 0;
	int32 ConnectedAliveCount = 0;
	int32 LiveDormantProxyCount = 0;
	int32 PhaseChangeCount = 0;
	EAuraEncounterPacingPhase Phase = EAuraEncounterPacingPhase::Build;
	double StartServerTime = 0.0;
	double PhaseStartServerTime = 0.0;
	double LastSampleServerTime = 0.0;
	double DeadlineServerTime = 0.0;
	double SmoothedIntensity = 0.0;
	bool bAuthority = false;
	bool bHasIntensitySample = false;
	bool bChoiceOrSupplyInteractionOpen = false;
	bool bAdmissionPaused = true;
	bool bLastInputClamped = false;
	FName TerminalReason = NAME_None;
	TArray<FAuraPacingRosterSlot> RosterSlots;
	TArray<int32> AdmissionOrder;
	TArray<FName> AdmissionHistory;
};
