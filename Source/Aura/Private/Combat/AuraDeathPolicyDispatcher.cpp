// Copyright Druid Mechanics

#include "Combat/AuraDeathPolicyDispatcher.h"

#include "Aura/AuraLogChannels.h"
#include "Game/AuraGameModeBase.h"

void UAuraDeathPolicyDispatcher::Initialize(AAuraGameModeBase* InGameMode)
{
	GameMode = InGameMode;
	HighestDispatchedSequenceByVictim.Reset();
	DispatchedDeathCount = 0;
}

bool UAuraDeathPolicyDispatcher::DispatchDeath(const FAuraDeathEvent& Event)
{
	if (!Event.IsValid() || !GameMode.IsValid() || !GameMode->HasAuthority())
	{
		return false;
	}

	const FObjectKey VictimKey(Event.VictimActor);
	if (const int32* HighestSequence = HighestDispatchedSequenceByVictim.Find(VictimKey);
		HighestSequence && Event.DeathSequence <= *HighestSequence)
	{
		UE_LOG(LogAura, Warning, TEXT("[DeathPolicy] Duplicate death event rejected victim=%s sequence=%d."),
			*GetNameSafe(Event.VictimActor), Event.DeathSequence);
		return false;
	}

	HighestDispatchedSequenceByVictim.Add(VictimKey, Event.DeathSequence);
	++DispatchedDeathCount;
	UE_LOG(LogAura, Display, TEXT("[DeathPolicy] AuthoritativeDeath victim=%s source=%s policy=%s sequence=%d zone=%s event=%s."),
		*GetNameSafe(Event.VictimActor), *GetNameSafe(Event.SourceActor), *Event.DeathPolicyTag.ToString(),
		Event.DeathSequence, *Event.BattleZoneId.ToString(), *Event.BattleEventId.ToString());
	OnAuthoritativeDeath.Broadcast(Event);
	return true;
}
