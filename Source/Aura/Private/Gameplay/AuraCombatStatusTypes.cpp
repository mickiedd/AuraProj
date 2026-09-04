// Copyright Druid Mechanics

#include "Gameplay/AuraCombatStatusTypes.h"

namespace AuraCombatStatusPrivate
{
	constexpr double InterruptImmunitySeconds = 2.0;

	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}
}

int32 FAuraCombatStatusLedger::FindStatusIndex(const FAuraCombatStatusKey& Key) const
{
	for (int32 Index = 0; Index < Statuses.Num(); ++Index)
	{
		if (Statuses[Index].Key == Key) return Index;
	}
	return INDEX_NONE;
}

int32 FAuraCombatStatusLedger::FindChannelIndex(const FAuraInterruptChannelKey& Key) const
{
	for (int32 Index = 0; Index < Channels.Num(); ++Index)
	{
		if (Channels[Index].Key == Key) return Index;
	}
	return INDEX_NONE;
}

EAuraCombatStatusResult FAuraCombatStatusLedger::TryApplyStatus(const FAuraCombatStatusKey& Key,
	FName StackingPolicy, FName InterruptClassification, double Now, double DurationSeconds, FString& OutError)
{
	OutError.Reset();
	if (!Key.IsValid() || StackingPolicy.IsNone() || !FMath::IsFinite(Now) || !FMath::IsFinite(DurationSeconds)
		|| DurationSeconds <= 0.0)
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("StatusApplicationInvalid"));
		return EAuraCombatStatusResult::Rejected;
	}
	const int32 ExistingIndex = FindStatusIndex(Key);
	if (Statuses.IsValidIndex(ExistingIndex))
	{
		const FAuraCombatStatusRecord& Existing = Statuses[ExistingIndex];
		if (Existing.StackingPolicy == StackingPolicy && Existing.InterruptClassification == InterruptClassification
			&& FMath::IsNearlyEqual(Existing.StartServerTime, Now)
			&& FMath::IsNearlyEqual(Existing.ExpiryServerTime, Now + DurationSeconds))
		{
			OutError = TEXT("DuplicateStatus");
			return EAuraCombatStatusResult::DuplicateAccepted;
		}
		AuraCombatStatusPrivate::Reject(OutError, TEXT("StatusIdentityConflict"));
		return EAuraCombatStatusResult::Rejected;
	}
	FAuraCombatStatusRecord& Record = Statuses.AddDefaulted_GetRef();
	Record.Key = Key;
	Record.StackingPolicy = StackingPolicy;
	Record.InterruptClassification = InterruptClassification;
	Record.StartServerTime = Now;
	Record.ExpiryServerTime = Now + DurationSeconds;
	return EAuraCombatStatusResult::Applied;
}

EAuraCombatStatusResult FAuraCombatStatusLedger::TryRemoveStatus(const FAuraCombatStatusKey& Key,
	EAuraCombatStatusRemovalReason Reason, FString& OutError)
{
	OutError.Reset();
	if (!Key.IsValid() || static_cast<uint8>(Reason) > static_cast<uint8>(EAuraCombatStatusRemovalReason::Manual))
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("StatusRemovalInvalid"));
		return EAuraCombatStatusResult::Rejected;
	}
	const int32 Index = FindStatusIndex(Key);
	if (!Statuses.IsValidIndex(Index))
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("StatusNotFound"));
		return EAuraCombatStatusResult::Rejected;
	}
	Statuses.RemoveAt(Index);
	return EAuraCombatStatusResult::Removed;
}

bool FAuraCombatStatusLedger::TryBeginInterruptibleChannel(const FAuraInterruptChannelKey& Key, double Now,
	double DurationSeconds, FString& OutError)
{
	OutError.Reset();
	if (!Key.IsValid() || !FMath::IsFinite(Now) || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0)
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("InterruptChannelInvalid"));
		return false;
	}
	const int32 ExistingIndex = FindChannelIndex(Key);
	if (Channels.IsValidIndex(ExistingIndex))
	{
		AuraCombatStatusPrivate::Reject(OutError, Channels[ExistingIndex].bActive
			? TEXT("InterruptChannelAlreadyActive") : TEXT("DuplicateInterruptChannel"));
		return false;
	}
	for (const FAuraInterruptChannelState& Channel : Channels)
	{
		if (Channel.Key.RunId == Key.RunId && Channel.Key.Epoch == Key.Epoch
			&& Channel.Key.EncounterGeneration == Key.EncounterGeneration
			&& Channel.Key.TargetEntityId == Key.TargetEntityId
			&& Channel.Key.TargetLifeGeneration == Key.TargetLifeGeneration
			&& Channel.InterruptImmuneUntilServerTime > Now)
		{
			AuraCombatStatusPrivate::Reject(OutError, TEXT("InterruptChannelImmune"));
			return false;
		}
	}
	FAuraInterruptChannelState& Channel = Channels.AddDefaulted_GetRef();
	Channel.Key = Key;
	Channel.StartServerTime = Now;
	Channel.EndServerTime = Now + DurationSeconds;
	Channel.InterruptImmuneUntilServerTime = 0.0;
	Channel.LastInterruptSequence = 0;
	Channel.bActive = true;
	return true;
}

EAuraInterruptResult FAuraCombatStatusLedger::TryInterruptChannel(const FAuraInterruptChannelKey& Key,
	int32 InterruptSequence, double Now, FString& OutError)
{
	OutError.Reset();
	if (!Key.IsValid() || InterruptSequence <= 0 || !FMath::IsFinite(Now))
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("InterruptRequestInvalid"));
		return EAuraInterruptResult::Rejected;
	}
	const int32 Index = FindChannelIndex(Key);
	if (!Channels.IsValidIndex(Index))
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("InterruptChannelNotFound"));
		return EAuraInterruptResult::Rejected;
	}
	FAuraInterruptChannelState& Channel = Channels[Index];
	if (InterruptSequence <= Channel.LastInterruptSequence)
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("DuplicateInterrupt"));
		return EAuraInterruptResult::Duplicate;
	}
	if (!Channel.bActive)
	{
		AuraCombatStatusPrivate::Reject(OutError, TEXT("InterruptChannelAlreadyEnded"));
		return EAuraInterruptResult::Rejected;
	}
	if (Now < Channel.InterruptImmuneUntilServerTime)
	{
		Channel.LastInterruptSequence = InterruptSequence;
		OutError = TEXT("InterruptImmune");
		return EAuraInterruptResult::Immune;
	}
	if (Now > Channel.EndServerTime)
	{
		Channel.bActive = false;
		AuraCombatStatusPrivate::Reject(OutError, TEXT("InterruptChannelExpired"));
		return EAuraInterruptResult::Rejected;
	}
	Channel.bActive = false;
	Channel.LastInterruptSequence = InterruptSequence;
	Channel.InterruptImmuneUntilServerTime = Now + AuraCombatStatusPrivate::InterruptImmunitySeconds;
	return EAuraInterruptResult::Interrupted;
}

int32 FAuraCombatStatusLedger::PruneExpired(double Now)
{
	if (!FMath::IsFinite(Now)) return 0;
	int32 Removed = 0;
	for (int32 Index = Statuses.Num() - 1; Index >= 0; --Index)
	{
		if (Statuses[Index].ExpiryServerTime <= Now)
		{
			Statuses.RemoveAt(Index);
			++Removed;
		}
	}
	for (int32 Index = Channels.Num() - 1; Index >= 0; --Index)
	{
		if (Channels[Index].EndServerTime <= Now
			|| (!Channels[Index].bActive && Channels[Index].InterruptImmuneUntilServerTime <= Now))
		{
			Channels.RemoveAt(Index);
			++Removed;
		}
	}
	return Removed;
}
