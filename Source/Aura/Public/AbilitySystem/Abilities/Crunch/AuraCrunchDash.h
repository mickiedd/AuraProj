// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchAbilityBase.h"
#include "AuraCrunchDash.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_ApplyRootMotionConstantForce;
class UAnimInstance;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;

/** Server-authoritative, CharacterMovement-compatible Crunch dash. */
UCLASS()
class AURA_API UAuraCrunchDash : public UAuraCrunchAbilityBase
{
	GENERATED_BODY()

public:
	UAuraCrunchDash();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Crunch Dash|Animation")
	TObjectPtr<UAnimMontage> DashMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Dash|Movement")
	float DashDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Dash|Movement")
	float DashDistance = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Dash|Targeting")
	float DashTargetRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch Dash|Damage")
	float DashPushSpeed = 3000.f;

private:
	UFUNCTION()
	void HandleDashStartEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleDashFinished();

	void StartDash();
	void TickDash();
	void StopDash();
	void HandleDashTimeout();

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DashStartEventTask;

	FTimerHandle DashStartTimer;
	FTimerHandle DashTickTimer;
	FTimerHandle DashTimeoutTimer;
	FVector DashDirection = FVector::ForwardVector;
	UPROPERTY()
	TObjectPtr<UAbilityTask_ApplyRootMotionConstantForce> DashMovementTask;
	TWeakObjectPtr<UAnimInstance> DashAnimInstance;
	uint8 PreviousRootMotionMode = 0;
	FVector DashStartLocation = FVector::ZeroVector;
	bool bDashStarted = false;
	bool bDashActive = false;
};
