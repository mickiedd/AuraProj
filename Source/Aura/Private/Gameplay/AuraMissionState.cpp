// Copyright Druid Mechanics

#include "Gameplay/AuraMissionState.h"

#include "Net/UnrealNetwork.h"

AAuraMissionState::AAuraMissionState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	SetNetUpdateFrequency(10.0f);
}

void AAuraMissionState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAuraMissionState, ReplicatedPublicSnapshot);
}

bool AAuraMissionState::PublishRunState(const FAuraMissionRunState& InState)
{
	const FAuraMissionPublicSnapshot Snapshot = InState.BuildPublicSnapshot();
	if (!HasAuthority())
	{
		return false;
	}
	if (ReplicatedPublicSnapshot.RunId.IsValid())
	{
		if (Snapshot.RunId == ReplicatedPublicSnapshot.RunId && Snapshot.Epoch == ReplicatedPublicSnapshot.Epoch)
		{
			if (Snapshot.Revision <= ReplicatedPublicSnapshot.Revision) return false;
		}
		else if (Snapshot.Epoch <= ReplicatedPublicSnapshot.Epoch)
		{
			// A new run must advance the authority epoch. This also rejects a
			// delayed snapshot from the previous run after a replacement run wins.
			return false;
		}
	}
	ReplicatedPublicSnapshot = Snapshot;
	// The authority gets the same notification as a replicated client, which keeps
	// the HUD/view-model binding independent of whether the actor is local.
	OnStateChanged.Broadcast(ReplicatedPublicSnapshot);
	ForceNetUpdate();
	return true;
}

void AAuraMissionState::OnRep_ReplicatedPublicSnapshot()
{
	OnStateChanged.Broadcast(ReplicatedPublicSnapshot);
}
