// Copyright Druid Mechanics

#include "Combat/AuraCombatStateComponent.h"

#include "Aura/AuraLogChannels.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UAuraCombatStateComponent::UAuraCombatStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAuraCombatStateComponent::BeginPlay()
{
	Super::BeginPlay();
	bInitialStateLocked = true;
	LastNotifiedState = LifeState;
}

void UAuraCombatStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAuraCombatStateComponent, LifeState);
}

UAuraCombatStateComponent* UAuraCombatStateComponent::FindForActor(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UAuraCombatStateComponent>() : nullptr;
}

const UAuraCombatStateComponent* UAuraCombatStateComponent::FindForActor(const AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UAuraCombatStateComponent>() : nullptr;
}

bool UAuraCombatStateComponent::TryTransition(EAuraCombatLifeState ExpectedState, EAuraCombatLifeState NewState)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || LifeState != ExpectedState)
	{
		return false;
	}

	LifeState = NewState;
	NotifyStateChanged();
	Owner->ForceNetUpdate();
	return true;
}

bool UAuraCombatStateComponent::TryEnterDying()
{
	return TryTransition(EAuraCombatLifeState::Alive, EAuraCombatLifeState::Dying);
}

bool UAuraCombatStateComponent::TryEnterDead()
{
	return TryTransition(EAuraCombatLifeState::Dying, EAuraCombatLifeState::Dead);
}

bool UAuraCombatStateComponent::TryEnterRespawning()
{
	return TryTransition(EAuraCombatLifeState::Dead, EAuraCombatLifeState::Respawning);
}

bool UAuraCombatStateComponent::TryEnterAlive()
{
	return TryTransition(EAuraCombatLifeState::Respawning, EAuraCombatLifeState::Alive);
}

bool UAuraCombatStateComponent::InitializeInitialState(EAuraCombatLifeState InitialState)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || bInitialStateLocked)
	{
		return false;
	}

	LifeState = InitialState;
	LastNotifiedState = InitialState;
	Owner->ForceNetUpdate();
	return true;
}

void UAuraCombatStateComponent::OnRep_LifeState()
{
	NotifyStateChanged();
}

void UAuraCombatStateComponent::NotifyStateChanged()
{
	if (LastNotifiedState == LifeState)
	{
		return;
	}

	LastNotifiedState = LifeState;
	OnLifeStateChanged.Broadcast(LifeState);
	UE_LOG(LogAura, Verbose, TEXT("[CombatState] Actor=%s LifeState=%d Authority=%s"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(LifeState),
		(IsValid(GetOwner()) && GetOwner()->HasAuthority()) ? TEXT("true") : TEXT("false"));
}
