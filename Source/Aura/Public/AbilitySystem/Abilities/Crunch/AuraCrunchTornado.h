// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchAbilityBase.h"
#include "AuraCrunchTornado.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;

/** Bounded, server-authored hit cadence for Crunch's Tornado. */
UCLASS()
class AURA_API UAuraCrunchTornado : public UAuraCrunchAbilityBase
{
	GENERATED_BODY()

public:
	UAuraCrunchTornado();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Crunch Tornado|Animation")
	TObjectPtr<UAnimMontage> TornadoMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Tornado|Timing")
	float TornadoDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Tornado|Timing")
	float HitInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Tornado|Targeting")
	float TornadoRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Tornado|Damage")
	float HitPushSpeed = 3000.f;

private:
	UFUNCTION()
	void HandleAuthoredDamageEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleTornadoTimeout();

	void ProcessTornadoHit(int32 EventIndex);
	void TickTornado();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DamageEventTask;

	FTimerHandle HitTimer;
	FTimerHandle TimeoutTimer;
	float TornadoElapsed = 0.f;
	int32 TornadoEventIndex = 0;
};
