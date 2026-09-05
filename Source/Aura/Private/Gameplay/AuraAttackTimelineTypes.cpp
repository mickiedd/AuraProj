// Copyright Druid Mechanics

#include "Gameplay/AuraAttackTimelineTypes.h"

namespace AuraAttackTimelinePrivate
{
	constexpr float MinimumWindupSeconds = 0.85f;
	constexpr double CallbackGraceSeconds = 1.0;

	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	bool IsDefinitionValid(const FAuraAttackTimelineDefinition& Definition, FString& OutError)
	{
		if (Definition.AttackDefinitionId.IsNone() || Definition.AttackShapeId.IsNone()
			|| Definition.TelegraphProfileId.IsNone())
		{
			Reject(OutError, TEXT("AttackDefinitionIdentityInvalid"));
			return false;
		}
		if (!FMath::IsFinite(Definition.WindupSeconds) || Definition.WindupSeconds < MinimumWindupSeconds)
		{
			Reject(OutError, TEXT("AttackWindupBelowContract"));
			return false;
		}
		if (!FMath::IsFinite(Definition.DamageWindowSeconds) || Definition.DamageWindowSeconds < 0.0f
			|| !FMath::IsFinite(Definition.RecoverySeconds) || Definition.RecoverySeconds < 0.0f)
		{
			Reject(OutError, TEXT("AttackTimingInvalid"));
			return false;
		}
		return true;
	}

	bool IsCallbackForActiveAttack(const FAuraAttackTimelineState& State, const FAuraAttackTimelineKey& CallbackKey,
		FString& OutError)
	{
		if (!CallbackKey.IsValid() || CallbackKey != State.Key)
		{
			Reject(OutError, TEXT("StaleAttackGeneration"));
			return false;
		}
		return true;
	}

	bool IsPastCallbackDeadline(const FAuraAttackTimelineState& State, double AuthorityNow)
	{
		return AuthorityNow > State.RecoveryEndServerTime + CallbackGraceSeconds + KINDA_SMALL_NUMBER;
	}

	EAuraAttackTimelineResult CancelLateCallback(FAuraAttackTimelineState& State, FString& OutError,
		const TCHAR* Reason)
	{
		State.Phase = EAuraAttackTimelinePhase::Cancelled;
		State.TerminalReason = FName(Reason);
		State.bImpactResolved = false;
		Reject(OutError, Reason);
		return EAuraAttackTimelineResult::Cancelled;
	}
}

EAuraAttackTimelineResult FAuraAttackTimelineState::TryBeginWindup(const FAuraAttackTimelineKey& InKey,
	const FAuraAttackTimelineDefinition& Definition, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (Phase != EAuraAttackTimelinePhase::Idle)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackTimelineBusy"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (!InKey.IsValid())
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackKeyInvalid"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (Key.IsValid() && InKey.RunId == Key.RunId && InKey.Epoch == Key.Epoch
		&& InKey.EncounterGeneration == Key.EncounterGeneration && InKey.EncounterId == Key.EncounterId
		&& InKey.DriverOwner == Key.DriverOwner && InKey.SourceEntityId == Key.SourceEntityId
		&& InKey.SourceLifeGeneration == Key.SourceLifeGeneration
		&& InKey.AttackSequence <= Key.AttackSequence)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackSequenceReplay"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (!FMath::IsFinite(AuthorityNow))
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AuthorityTimeInvalid"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (!AuraAttackTimelinePrivate::IsDefinitionValid(Definition, OutError))
	{
		return EAuraAttackTimelineResult::Rejected;
	}

	Key = InKey;
	AttackDefinitionId = Definition.AttackDefinitionId;
	AttackShapeId = Definition.AttackShapeId;
	TelegraphProfileId = Definition.TelegraphProfileId;
	Phase = EAuraAttackTimelinePhase::Windup;
	AuthorityStartServerTime = AuthorityNow;
	TelegraphStartServerTime = AuthorityNow;
	ExpectedImpactServerTime = AuthorityNow + static_cast<double>(Definition.WindupSeconds);
	DamageWindowEndServerTime = ExpectedImpactServerTime + static_cast<double>(Definition.DamageWindowSeconds);
	RecoveryEndServerTime = DamageWindowEndServerTime + static_cast<double>(Definition.RecoverySeconds);
	TerminalReason = NAME_None;
	bImpactResolved = false;
	return EAuraAttackTimelineResult::Started;
}

EAuraAttackTimelineResult FAuraAttackTimelineState::TryCommit(const FAuraAttackTimelineKey& CallbackKey,
	double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!AuraAttackTimelinePrivate::IsCallbackForActiveAttack(*this, CallbackKey, OutError))
	{
		return EAuraAttackTimelineResult::Rejected;
	}
	if (Phase != EAuraAttackTimelinePhase::Windup)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackCommitRejected"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (!FMath::IsFinite(AuthorityNow))
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AuthorityTimeInvalid"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (AuraAttackTimelinePrivate::IsPastCallbackDeadline(*this, AuthorityNow))
	{
		return AuraAttackTimelinePrivate::CancelLateCallback(*this, OutError, TEXT("AttackCallbackTimeout"));
	}
	if (AuthorityNow + KINDA_SMALL_NUMBER < ExpectedImpactServerTime)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackImpactTooEarly"));
		return EAuraAttackTimelineResult::Rejected;
	}
	Phase = EAuraAttackTimelinePhase::Committed;
	return EAuraAttackTimelineResult::Committed;
}

EAuraAttackTimelineResult FAuraAttackTimelineState::TryResolveImpact(const FAuraAttackTimelineKey& CallbackKey,
	double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!AuraAttackTimelinePrivate::IsCallbackForActiveAttack(*this, CallbackKey, OutError))
	{
		return EAuraAttackTimelineResult::Rejected;
	}
	if (Phase != EAuraAttackTimelinePhase::Committed || bImpactResolved)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackImpactRejected"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (!FMath::IsFinite(AuthorityNow))
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AuthorityTimeInvalid"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (AuraAttackTimelinePrivate::IsPastCallbackDeadline(*this, AuthorityNow))
	{
		return AuraAttackTimelinePrivate::CancelLateCallback(*this, OutError, TEXT("AttackCallbackTimeout"));
	}
	if (AuthorityNow + KINDA_SMALL_NUMBER < ExpectedImpactServerTime)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackImpactTooEarly"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (AuthorityNow > DamageWindowEndServerTime + KINDA_SMALL_NUMBER)
	{
		return AuraAttackTimelinePrivate::CancelLateCallback(*this, OutError, TEXT("AttackImpactWindowExpired"));
	}
	bImpactResolved = true;
	Phase = EAuraAttackTimelinePhase::Recovery;
	return EAuraAttackTimelineResult::ImpactResolved;
}

EAuraAttackTimelineResult FAuraAttackTimelineState::TryFinishRecovery(const FAuraAttackTimelineKey& CallbackKey,
	double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!AuraAttackTimelinePrivate::IsCallbackForActiveAttack(*this, CallbackKey, OutError))
	{
		return EAuraAttackTimelineResult::Rejected;
	}
	if (Phase != EAuraAttackTimelinePhase::Recovery)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackRecoveryRejected"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (!FMath::IsFinite(AuthorityNow))
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AuthorityTimeInvalid"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (AuraAttackTimelinePrivate::IsPastCallbackDeadline(*this, AuthorityNow))
	{
		return AuraAttackTimelinePrivate::CancelLateCallback(*this, OutError, TEXT("AttackCallbackTimeout"));
	}
	if (AuthorityNow + KINDA_SMALL_NUMBER < RecoveryEndServerTime)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackRecoveryTooEarly"));
		return EAuraAttackTimelineResult::Rejected;
	}
	Phase = EAuraAttackTimelinePhase::Idle;
	return EAuraAttackTimelineResult::RecoveryFinished;
}

EAuraAttackTimelineResult FAuraAttackTimelineState::TryCancel(const FAuraAttackTimelineKey& CallbackKey,
	FName Reason, FString& OutError)
{
	OutError.Reset();
	if (!AuraAttackTimelinePrivate::IsCallbackForActiveAttack(*this, CallbackKey, OutError))
	{
		return EAuraAttackTimelineResult::Rejected;
	}
	if (Phase != EAuraAttackTimelinePhase::Windup && Phase != EAuraAttackTimelinePhase::Committed)
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackCancellationRejected"));
		return EAuraAttackTimelineResult::Rejected;
	}
	if (Reason.IsNone())
	{
		AuraAttackTimelinePrivate::Reject(OutError, TEXT("AttackCancellationReasonMissing"));
		return EAuraAttackTimelineResult::Rejected;
	}
	Phase = EAuraAttackTimelinePhase::Cancelled;
	TerminalReason = Reason;
	bImpactResolved = false;
	return EAuraAttackTimelineResult::Cancelled;
}

void FAuraAttackTimelineState::Reset()
{
	if (Phase != EAuraAttackTimelinePhase::Cancelled) return;
	*this = FAuraAttackTimelineState();
}
