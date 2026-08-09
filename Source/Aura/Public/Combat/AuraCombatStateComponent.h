// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/AuraCombatTypes.h"
#include "AuraCombatStateComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FAuraCombatLifeStateChanged, EAuraCombatLifeState);

/**
 * Replicated, authority-owned life state used by all combat rules. Character
 * avatars begin in Respawning and explicitly become Alive after their normal
 * gameplay initialization has completed.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent), DisplayName = "Aura Combat State")
class AURA_API UAuraCombatStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraCombatStateComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	EAuraCombatLifeState GetLifeState() const { return LifeState; }
	bool IsAlive() const { return LifeState == EAuraCombatLifeState::Alive; }

	/** Generic lookup for actors that own a combat-state component. */
	static UAuraCombatStateComponent* FindForActor(AActor* Actor);
	static const UAuraCombatStateComponent* FindForActor(const AActor* Actor);

	/** Authority-only transition winner. Returns true only for Alive -> Dying. */
	bool TryEnterDying();

	/** Authority-only presentation/lifecycle transitions. */
	bool TryEnterDead();
	bool TryEnterRespawning();
	bool TryEnterAlive();

	/**
	 * Authority-only initial-state hook for deferred/test spawns. It may only be
	 * used before the component has begun play and never acts as a live setter.
	 */
	bool InitializeInitialState(EAuraCombatLifeState InitialState);

	FAuraCombatLifeStateChanged OnLifeStateChanged;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_LifeState();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_LifeState, Category = "Combat State")
	EAuraCombatLifeState LifeState = EAuraCombatLifeState::Respawning;

private:
	bool TryTransition(EAuraCombatLifeState ExpectedState, EAuraCombatLifeState NewState);
	void NotifyStateChanged();

	EAuraCombatLifeState LastNotifiedState = EAuraCombatLifeState::Respawning;
	bool bInitialStateLocked = false;
};
