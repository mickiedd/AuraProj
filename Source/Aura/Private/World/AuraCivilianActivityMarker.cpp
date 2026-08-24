// Copyright Druid Mechanics

#include "World/AuraCivilianActivityMarker.h"

#include "Game/AuraGameModeBase.h"
#include "World/AuraPopulationManager.h"

AAuraCivilianActivityMarker::AAuraCivilianActivityMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AAuraCivilianActivityMarker::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithPopulationManager();
}

void AAuraCivilianActivityMarker::RegisterWithPopulationManager()
{
	if (!HasAuthority()) return;
	if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
	{
		if (UAuraPopulationManager* Manager = GameMode->GetPopulationManagerMutable())
		{
			Manager->RegisterActivityMarker(this);
		}
	}
}

void AAuraCivilianActivityMarker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (AAuraGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr)
		{
			if (UAuraPopulationManager* Manager = GameMode->GetPopulationManagerMutable())
			{
				Manager->UnregisterActivityMarker(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AAuraCivilianActivityMarker::SetMarkerIdForRuntime(FName InMarkerId)
{
	if (HasAuthority() && !InMarkerId.IsNone())
	{
		MarkerId = InMarkerId;
		if (HasActorBegunPlay()) RegisterWithPopulationManager();
	}
}

void AAuraCivilianActivityMarker::SetZoneIdForRuntime(FName InZoneId)
{
	if (HasAuthority() && !InZoneId.IsNone())
	{
		ZoneId = InZoneId;
		if (HasActorBegunPlay()) RegisterWithPopulationManager();
	}
}

bool AAuraCivilianActivityMarker::CanAccept(FName WorkProfileId, const FGameplayTagContainer& RequiredTags) const
{
	return bEnabled && !MarkerId.IsNone() && (RequiredTags.IsEmpty() || RequiredGameplayTags.HasAll(RequiredTags))
		&& Capacity > 0 && !WorkProfileId.IsNone();
}
