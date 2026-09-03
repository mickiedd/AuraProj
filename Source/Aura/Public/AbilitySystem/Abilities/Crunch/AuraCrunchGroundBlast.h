// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchAbilityBase.h"
#include "AuraCrunchGroundBlast.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAnimMontage;
class UTargetDataUnderMouse;

/** Ground-targeted Crunch blast with an authority-side retrace and radial query. */
UCLASS()
class AURA_API UAuraCrunchGroundBlast : public UAuraCrunchAbilityBase
{
	GENERATED_BODY()

public:
	UAuraCrunchGroundBlast();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Crunch GroundBlast|Animation")
	TObjectPtr<UAnimMontage> TargetingMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch GroundBlast|Animation")
	TObjectPtr<UAnimMontage> CastMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch GroundBlast|Targeting")
	float TargetAreaRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch GroundBlast|Targeting")
	float TargetTraceRange = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch GroundBlast|Targeting")
	float MaxTargetHeight = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch GroundBlast|Damage")
	float TargetPushSpeed = 3000.f;

private:
	UFUNCTION()
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& DataHandle);

	UFUNCTION()
	void HandleTargetTimeout();

	bool ValidateGroundTarget(const FHitResult& HitResult, FVector& OutPoint) const;
	void CommitGroundBlast(const FVector& Point);

	UPROPERTY()
	TObjectPtr<UTargetDataUnderMouse> TargetDataTask;

	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle TargetTimeoutTimer;
	bool bCommitted = false;
};
