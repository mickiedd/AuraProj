// Copyright Druid Mechanics

#include "Gameplay/AuraEncounterPacingTypes.h"

namespace AuraEncounterPacingPrivate
{
	constexpr double SchedulerIntervalSeconds = 0.5;
	constexpr double IntensitySmoothingWindowSeconds = 2.0;
	constexpr double RecoveryEnterIntensity = 0.75;
	constexpr double RecoveryExitIntensity = 0.45;
	constexpr double MinimumBuildSeconds = 10.0;
	constexpr double MaximumPressureSeconds = 30.0;
	constexpr double MinimumRecoverySeconds = 8.0;
	constexpr double MaximumRecoverySeconds = 20.0;

	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	double ClampSignal(double Value, bool& bOutClamped)
	{
		if (!FMath::IsFinite(Value))
		{
			bOutClamped = true;
			return 0.0;
		}
		const double Clamped = FMath::Clamp(Value, 0.0, 1.0);
		bOutClamped |= !FMath::IsNearlyEqual(Value, Clamped);
		return Clamped;
	}

	uint64 NextValue(uint64& State)
	{
		State ^= State << 13;
		State ^= State >> 7;
		State ^= State << 17;
		return State;
	}
}

bool FAuraEncounterPacingSnapshot::operator==(const FAuraEncounterPacingSnapshot& Other) const
{
	return RunId == Other.RunId && Epoch == Other.Epoch && Revision == Other.Revision && Phase == Other.Phase
		&& FMath::IsNearlyEqual(PhaseStartServerTime, Other.PhaseStartServerTime)
		&& FMath::IsNearlyEqual(DeadlineServerTime, Other.DeadlineServerTime)
		&& FMath::IsNearlyEqual(SmoothedIntensity, Other.SmoothedIntensity)
		&& CurrentWaveCost == Other.CurrentWaveCost && WaveAdmissionBudget == Other.WaveAdmissionBudget
		&& LiveHostileCount == Other.LiveHostileCount && PendingRequiredCount == Other.PendingRequiredCount
		&& ConnectedAliveCount == Other.ConnectedAliveCount && LiveDormantProxyCount == Other.LiveDormantProxyCount
		&& PhaseChangeCount == Other.PhaseChangeCount && bAdmissionPaused == Other.bAdmissionPaused
		&& bLastInputClamped == Other.bLastInputClamped && TerminalReason == Other.TerminalReason
		&& AdmissionHistory == Other.AdmissionHistory;
}

int32 FAuraEncounterPacingState::CostForArchetype(FName ArchetypeId)
{
	if (ArchetypeId == TEXT("Raider")) return 1;
	if (ArchetypeId == TEXT("Lancer")) return 2;
	if (ArchetypeId == TEXT("Bulwark") || ArchetypeId == TEXT("Disruptor")) return 3;
	return 0;
}

bool FAuraEncounterPacingState::Initialize(bool bInAuthority, const FGuid& InRunId, int32 InEpoch,
	int32 InParticipantCount, int32 InSeed, double InStartServerTime, double InDeadlineSeconds,
	const TArray<FAuraPacingRosterSlot>& InRequiredRoster, FString& OutError)
{
	OutError.Reset();
	*this = FAuraEncounterPacingState();
	if (!bInAuthority || !InRunId.IsValid() || InEpoch <= 0 || (InParticipantCount != 1 && InParticipantCount != 2)
		|| InSeed <= 0 || !FMath::IsFinite(InStartServerTime) || !FMath::IsFinite(InDeadlineSeconds)
		|| InDeadlineSeconds <= 0.0 || InStartServerTime > TNumericLimits<double>::Max() - InDeadlineSeconds
		|| InRequiredRoster.IsEmpty())
	{
		AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingIdentityInvalid"));
		return false;
	}

	const int32 CandidateBudget = InParticipantCount == 1 ? 6 : 10;
	TSet<FName> SeenSlots;
	int32 TotalCost = 0;
	for (const FAuraPacingRosterSlot& Slot : InRequiredRoster)
	{
		const int32 ExpectedCost = CostForArchetype(Slot.ArchetypeId);
		if (Slot.SlotId.IsNone() || SeenSlots.Contains(Slot.SlotId) || !Slot.bRequired || Slot.bAdmitted
			|| Slot.bTerminal || Slot.Generation <= 0 || Slot.SourceLifeGeneration <= 0 || ExpectedCost <= 0
			|| Slot.Cost != ExpectedCost || Slot.Cost > CandidateBudget)
		{
			AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingRosterInvalid"));
			return false;
		}
		SeenSlots.Add(Slot.SlotId);
		TotalCost += Slot.Cost;
	}
	const int32 MinimumWaveCount = FMath::DivideAndRoundUp(TotalCost, CandidateBudget);
	const double MinimumScheduleSeconds = MinimumWaveCount * AuraEncounterPacingPrivate::MinimumBuildSeconds
		+ FMath::Max(0, MinimumWaveCount - 1) * AuraEncounterPacingPrivate::MinimumRecoverySeconds;
	if (InDeadlineSeconds + KINDA_SMALL_NUMBER < MinimumScheduleSeconds)
	{
		AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingRosterDeadlineImpossible"));
		return false;
	}

	bAuthority = true;
	RunId = InRunId;
	Epoch = InEpoch;
	ParticipantCount = InParticipantCount;
	Seed = InSeed;
	WaveAdmissionBudget = CandidateBudget;
	ActiveHostileCap = InParticipantCount == 1 ? 10 : 16;
	StartServerTime = InStartServerTime;
	PhaseStartServerTime = InStartServerTime;
	LastSampleServerTime = InStartServerTime - AuraEncounterPacingPrivate::SchedulerIntervalSeconds;
	DeadlineServerTime = InStartServerTime + InDeadlineSeconds;
	RosterSlots = InRequiredRoster;
	RosterSlots.Sort([](const FAuraPacingRosterSlot& A, const FAuraPacingRosterSlot& B)
	{
		return A.SlotId.ToString() < B.SlotId.ToString();
	});
	AdmissionOrder.Reserve(RosterSlots.Num());
	for (int32 Index = 0; Index < RosterSlots.Num(); ++Index) AdmissionOrder.Add(Index);
	uint64 StreamState = static_cast<uint64>(static_cast<uint32>(Seed)) | 1ULL;
	for (int32 Index = AdmissionOrder.Num() - 1; Index > 0; --Index)
	{
		const int32 SwapIndex = static_cast<int32>(AuraEncounterPacingPrivate::NextValue(StreamState)
			% static_cast<uint64>(Index + 1));
		AdmissionOrder.Swap(Index, SwapIndex);
	}
	bAdmissionPaused = true;
	return Touch(OutError);
}

bool FAuraEncounterPacingState::Touch(FString& OutError)
{
	if (Revision == MAX_int32)
	{
		AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingRevisionOverflow"));
		return false;
	}
	++Revision;
	return true;
}

void FAuraEncounterPacingState::TransitionTo(EAuraEncounterPacingPhase NewPhase, double AuthorityNow)
{
	if (Phase == NewPhase) return;
	Phase = NewPhase;
	PhaseStartServerTime = AuthorityNow;
	++PhaseChangeCount;
	if (NewPhase == EAuraEncounterPacingPhase::Build) CurrentWaveCost = 0;
}

EAuraPacingMutationResult FAuraEncounterPacingState::Abort(FName Reason, FString& OutError)
{
	TerminalReason = Reason;
	Phase = EAuraEncounterPacingPhase::Aborted;
	bAdmissionPaused = true;
	Touch(OutError);
	return EAuraPacingMutationResult::TechnicalAbort;
}

EAuraPacingMutationResult FAuraEncounterPacingState::Tick(bool bInAuthority, double AuthorityNow,
	double RecentHealthLossFraction, double NearbyCommittedThreatFraction,
	const TArray<FAuraPacingParticipantState>& Participants, bool bInChoiceOrSupplyInteractionOpen,
	FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || Phase == EAuraEncounterPacingPhase::Aborted
		|| !FMath::IsFinite(AuthorityNow) || AuthorityNow + KINDA_SMALL_NUMBER < LastSampleServerTime
			+ AuraEncounterPacingPrivate::SchedulerIntervalSeconds)
	{
		AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingTickRejected"));
		return EAuraPacingMutationResult::Rejected;
	}
	if (!IsRequiredRosterComplete() && AuthorityNow >= DeadlineServerTime - KINDA_SMALL_NUMBER)
	{
		return Abort(TEXT("RequiredRosterDeadlineExpired"), OutError);
	}

	bool bCandidateInputClamped = false;
	const double HealthLoss = AuraEncounterPacingPrivate::ClampSignal(
		RecentHealthLossFraction, bCandidateInputClamped);
	const double Threats = AuraEncounterPacingPrivate::ClampSignal(
		NearbyCommittedThreatFraction, bCandidateInputClamped);
	int32 CandidateConnectedAliveCount = 0;
	int32 CandidateLiveDormantProxyCount = 0;
	int32 IncapacitatedCount = 0;
	TSet<FName> SeenParticipants;
	for (const FAuraPacingParticipantState& Participant : Participants)
	{
		if (Participant.ParticipantId.IsNone() || SeenParticipants.Contains(Participant.ParticipantId))
		{
			AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingParticipantIdentityInvalid"));
			return EAuraPacingMutationResult::Rejected;
		}
		SeenParticipants.Add(Participant.ParticipantId);
		AuraEncounterPacingPrivate::ClampSignal(Participant.HealthNormalized, bCandidateInputClamped);
		if (Participant.bConnected && Participant.bAlive) ++CandidateConnectedAliveCount;
		if (!Participant.bConnected && Participant.bAlive && Participant.bTargetableDormantProxy)
			++CandidateLiveDormantProxyCount;
		if (!Participant.bAlive) ++IncapacitatedCount;
	}
	bLastInputClamped = bCandidateInputClamped;
	ConnectedAliveCount = CandidateConnectedAliveCount;
	LiveDormantProxyCount = CandidateLiveDormantProxyCount;
	const double Incapacitated = Participants.IsEmpty() ? 0.0
		: static_cast<double>(IncapacitatedCount) / static_cast<double>(Participants.Num());
	const double RawIntensity = FMath::Max3(HealthLoss, Threats, Incapacitated);
	const double DeltaSeconds = bHasIntensitySample ? AuthorityNow - LastSampleServerTime
		: AuraEncounterPacingPrivate::IntensitySmoothingWindowSeconds;
	const double Alpha = FMath::Clamp(DeltaSeconds / AuraEncounterPacingPrivate::IntensitySmoothingWindowSeconds,
		0.0, 1.0);
	SmoothedIntensity = bHasIntensitySample
		? FMath::Lerp(SmoothedIntensity, RawIntensity, Alpha) : RawIntensity;
	bHasIntensitySample = true;
	LastSampleServerTime = AuthorityNow;
	bChoiceOrSupplyInteractionOpen = bInChoiceOrSupplyInteractionOpen;
	bAdmissionPaused = bChoiceOrSupplyInteractionOpen || ConnectedAliveCount == 0;

	const double PhaseSeconds = AuthorityNow - PhaseStartServerTime;
	EAuraEncounterPacingPhase NextPhase = Phase;
	if (Phase == EAuraEncounterPacingPhase::Build
		&& PhaseSeconds + KINDA_SMALL_NUMBER >= AuraEncounterPacingPrivate::MinimumBuildSeconds
		&& !bAdmissionPaused && !IsRequiredRosterComplete())
	{
		NextPhase = EAuraEncounterPacingPhase::Pressure;
	}
	else if (Phase == EAuraEncounterPacingPhase::Pressure
		&& (SmoothedIntensity + KINDA_SMALL_NUMBER >= AuraEncounterPacingPrivate::RecoveryEnterIntensity
			|| PhaseSeconds + KINDA_SMALL_NUMBER >= AuraEncounterPacingPrivate::MaximumPressureSeconds))
	{
		NextPhase = EAuraEncounterPacingPhase::Recovery;
	}
	else if (Phase == EAuraEncounterPacingPhase::Recovery && !bChoiceOrSupplyInteractionOpen
		&& ((PhaseSeconds + KINDA_SMALL_NUMBER >= AuraEncounterPacingPrivate::MinimumRecoverySeconds
				&& SmoothedIntensity <= AuraEncounterPacingPrivate::RecoveryExitIntensity + KINDA_SMALL_NUMBER)
			|| PhaseSeconds + KINDA_SMALL_NUMBER >= AuraEncounterPacingPrivate::MaximumRecoverySeconds))
	{
		NextPhase = EAuraEncounterPacingPhase::Build;
	}

	if (NextPhase != Phase)
	{
		TransitionTo(NextPhase, AuthorityNow);
		return Touch(OutError) ? EAuraPacingMutationResult::PhaseChanged : EAuraPacingMutationResult::Rejected;
	}
	return Touch(OutError) ? EAuraPacingMutationResult::NoChange : EAuraPacingMutationResult::Rejected;
}

EAuraPacingMutationResult FAuraEncounterPacingState::TryAdmitNext(bool bInAuthority, double AuthorityNow,
	FName& OutSlotId, FString& OutError)
{
	OutError.Reset();
	OutSlotId = NAME_None;
	if (!bAuthority || !bInAuthority || Phase == EAuraEncounterPacingPhase::Aborted
		|| !FMath::IsFinite(AuthorityNow))
	{
		AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingAdmissionRejected"));
		return EAuraPacingMutationResult::Rejected;
	}
	if (IsRequiredRosterComplete()) return EAuraPacingMutationResult::RosterComplete;
	if (AuthorityNow >= DeadlineServerTime - KINDA_SMALL_NUMBER)
		return Abort(TEXT("RequiredRosterDeadlineExpired"), OutError);
	if (Phase != EAuraEncounterPacingPhase::Pressure || bAdmissionPaused)
		return EAuraPacingMutationResult::AdmissionDeferred;
	if (GetLiveHostileCount() >= ActiveHostileCap)
		return EAuraPacingMutationResult::AdmissionDeferred;

	for (const int32 Index : AdmissionOrder)
	{
		if (!RosterSlots.IsValidIndex(Index)) continue;
		FAuraPacingRosterSlot& Slot = RosterSlots[Index];
		if (Slot.bAdmitted || Slot.bTerminal || CurrentWaveCost + Slot.Cost > WaveAdmissionBudget) continue;
		Slot.bAdmitted = true;
		CurrentWaveCost += Slot.Cost;
		OutSlotId = Slot.SlotId;
		AdmissionHistory.Add(Slot.SlotId);
		return Touch(OutError) ? EAuraPacingMutationResult::AdmissionGranted : EAuraPacingMutationResult::Rejected;
	}
	return EAuraPacingMutationResult::AdmissionDeferred;
}

EAuraPacingMutationResult FAuraEncounterPacingState::MarkSlotTerminal(bool bInAuthority, FName SlotId,
	int32 Generation, int32 SourceLifeGeneration, FString& OutError)
{
	OutError.Reset();
	const int32 Index = FindSlotIndex(SlotId);
	if (!bAuthority || !bInAuthority || !RosterSlots.IsValidIndex(Index) || Generation <= 0
		|| SourceLifeGeneration <= 0 || !RosterSlots[Index].bAdmitted || RosterSlots[Index].bTerminal
		|| RosterSlots[Index].Generation != Generation
		|| RosterSlots[Index].SourceLifeGeneration != SourceLifeGeneration)
	{
		AuraEncounterPacingPrivate::Reject(OutError, TEXT("PacingTerminalReceiptRejected"));
		return EAuraPacingMutationResult::Rejected;
	}
	RosterSlots[Index].bTerminal = true;
	if (!Touch(OutError)) return EAuraPacingMutationResult::Rejected;
	return IsRequiredRosterComplete() ? EAuraPacingMutationResult::RosterComplete
		: EAuraPacingMutationResult::SlotTerminal;
}

int32 FAuraEncounterPacingState::GetLiveHostileCount() const
{
	int32 Count = 0;
	for (const FAuraPacingRosterSlot& Slot : RosterSlots)
		if (Slot.bAdmitted && !Slot.bTerminal) ++Count;
	return Count;
}

int32 FAuraEncounterPacingState::GetPendingRequiredCount() const
{
	int32 Count = 0;
	for (const FAuraPacingRosterSlot& Slot : RosterSlots)
		if (Slot.bRequired && !Slot.bAdmitted && !Slot.bTerminal) ++Count;
	return Count;
}

int32 FAuraEncounterPacingState::FindSlotIndex(FName SlotId) const
{
	for (int32 Index = 0; Index < RosterSlots.Num(); ++Index)
		if (RosterSlots[Index].SlotId == SlotId) return Index;
	return INDEX_NONE;
}

bool FAuraEncounterPacingState::IsRequiredRosterComplete() const
{
	if (RosterSlots.IsEmpty()) return false;
	for (const FAuraPacingRosterSlot& Slot : RosterSlots)
		if (Slot.bRequired && !Slot.bTerminal) return false;
	return true;
}

FAuraEncounterPacingSnapshot FAuraEncounterPacingState::BuildSnapshot() const
{
	FAuraEncounterPacingSnapshot Snapshot;
	Snapshot.RunId = RunId;
	Snapshot.Epoch = Epoch;
	Snapshot.Revision = Revision;
	Snapshot.Phase = Phase;
	Snapshot.PhaseStartServerTime = PhaseStartServerTime;
	Snapshot.DeadlineServerTime = DeadlineServerTime;
	Snapshot.SmoothedIntensity = SmoothedIntensity;
	Snapshot.CurrentWaveCost = CurrentWaveCost;
	Snapshot.WaveAdmissionBudget = WaveAdmissionBudget;
	Snapshot.LiveHostileCount = GetLiveHostileCount();
	Snapshot.PendingRequiredCount = GetPendingRequiredCount();
	Snapshot.ConnectedAliveCount = ConnectedAliveCount;
	Snapshot.LiveDormantProxyCount = LiveDormantProxyCount;
	Snapshot.PhaseChangeCount = PhaseChangeCount;
	Snapshot.bAdmissionPaused = bAdmissionPaused;
	Snapshot.bLastInputClamped = bLastInputClamped;
	Snapshot.TerminalReason = TerminalReason;
	Snapshot.AdmissionHistory = AdmissionHistory;
	return Snapshot;
}
