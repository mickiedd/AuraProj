// Copyright Druid Mechanics

#include "Combat/AuraCombatIdentityComponent.h"

#include "Aura/AuraLogChannels.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

namespace AuraCombatIdentityPrivate
{
	TSet<TWeakObjectPtr<AActor>> LoggedMissingIdentityActors;
	bool bLoggedNullIdentityActor = false;
}

UAuraCombatIdentityComponent::UAuraCombatIdentityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAuraCombatIdentityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UAuraCombatIdentityComponent, Identity);
}

bool UAuraCombatIdentityComponent::InitializeIdentity(const FAuraCombatIdentity& InIdentity)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		UE_LOG(LogAura, Warning, TEXT("[CombatIdentity] Rejected non-authority initialization for %s."), *GetNameSafe(Owner));
		return false;
	}

	if (!InIdentity.IsValid())
	{
		LogMissingIdentityOnce(Owner, TEXT("InitializeIdentity"));
		return false;
	}
	if (Identity == InIdentity)
	{
		return true;
	}

	Identity = InIdentity;
	Owner->ForceNetUpdate();
	OnIdentityChanged.Broadcast(Identity);
	LogIdentity(TEXT("Server"));
	return true;
}

void UAuraCombatIdentityComponent::RestoreIdentityForRollback(const FAuraCombatIdentity& InIdentity, bool bWasValid)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	if (bWasValid)
	{
		InitializeIdentity(InIdentity);
		return;
	}
	if (!Identity.IsValid())
	{
		return;
	}
	Identity = FAuraCombatIdentity();
	Owner->ForceNetUpdate();
	OnIdentityChanged.Broadcast(Identity);
	LogIdentity(TEXT("ServerRollback"));
}

UAuraCombatIdentityComponent* UAuraCombatIdentityComponent::FindForActor(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UAuraCombatIdentityComponent>() : nullptr;
}

const UAuraCombatIdentityComponent* UAuraCombatIdentityComponent::FindForActor(const AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UAuraCombatIdentityComponent>() : nullptr;
}

void UAuraCombatIdentityComponent::LogMissingIdentityOnce(const AActor* Actor, const TCHAR* Context)
{
	using namespace AuraCombatIdentityPrivate;

	if (!IsValid(Actor))
	{
		if (!bLoggedNullIdentityActor)
		{
			bLoggedNullIdentityActor = true;
			UE_LOG(LogAura, Warning, TEXT("[CombatIdentity] %s rejected a null actor."), Context ? Context : TEXT("Unknown"));
		}
		return;
	}

	if (LoggedMissingIdentityActors.Num() > 128)
	{
		for (auto It = LoggedMissingIdentityActors.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	AActor* MutableActor = const_cast<AActor*>(Actor);
	const TWeakObjectPtr<AActor> ActorKey(MutableActor);
	if (!LoggedMissingIdentityActors.Contains(ActorKey))
	{
		LoggedMissingIdentityActors.Add(ActorKey);
		UE_LOG(LogAura, Warning, TEXT("[CombatIdentity] %s rejected actor %s because its combat identity is missing or invalid."),
			Context ? Context : TEXT("Unknown"), *GetNameSafe(Actor));
	}
}

void UAuraCombatIdentityComponent::OnRep_Identity()
{
	OnIdentityChanged.Broadcast(Identity);
	LogIdentity(TEXT("Client"));
}

void UAuraCombatIdentityComponent::LogIdentity(const TCHAR* NetworkSide) const
{
	UE_LOG(LogAura, Display,
		TEXT("[CombatIdentity][%s] Actor=%s Faction=%s Control=%s Profile=%s Death=%s Targetable=%d CanAttack=%d CanBeDamaged=%d FriendlyFire=%d"),
		NetworkSide,
		*GetNameSafe(GetOwner()),
		*Identity.FactionTag.ToString(),
		*Identity.ControlTypeTag.ToString(),
		*Identity.CombatProfileTag.ToString(),
		*Identity.DeathPolicyTag.ToString(),
		Identity.bTargetable,
		Identity.bCanAttack,
		Identity.bCanBeDamaged,
		Identity.bAllowFriendlyFire);
}
