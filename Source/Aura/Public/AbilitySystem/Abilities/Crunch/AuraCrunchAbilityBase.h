// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"
#include "AuraCrunchAbilityBase.generated.h"

class ACharacter;
class UAbilityTask;
class UCharacterMovementComponent;

/**
 * Shared authority boundary for the Crunch skill set.
 *
 * The source project used several unrelated target actors and GameplayEffects.  Aura
 * deliberately keeps one native boundary instead: every impact revalidates the target
 * through FAuraCombatRules and every activation owns an idempotent hit ledger/timer
 * cleanup path.  Derived skills only describe their movement and authored timing.
 */
UCLASS(Abstract)
class AURA_API UAuraCrunchAbilityBase : public UAuraDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UAuraCrunchAbilityBase();

	virtual FGameplayTag GetStartupInputTag() const override;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Native tag names are retained as FNames because ability CDOs can be constructed before native tags register. */
	FName CrunchStartupInputTagName;
	/** Called by derived abilities after their actor-info/prediction checks. */
	bool BeginCrunchActivation(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo);

	/** Clears all registered timers and the per-activation hit ledger. */
	void CleanupCrunchActivation();
	void RegisterCrunchTimer(const FTimerHandle& Handle);

	/** Server-side target validation used both at launch and at every impact. */
	bool ValidateCrunchTarget(AActor* Target, float MaxRange, FString* OutReason = nullptr) const;
	void CollectCrunchTargets(const FVector& Origin, float Radius, TArray<AActor*>& OutTargets, float MaxRange = 0.f) const;

	/** Applies one authoritative impact. EventKey scopes dedupe to an authored event. */
	bool ApplyCrunchDamageOnce(AActor* Target, FName EventKey, float DamageOverride = 0.f,
		const FVector& KnockbackVelocity = FVector::ZeroVector, const FGameplayTag& DamageTypeOverride = FGameplayTag());

	void LaunchCrunchCharacter(AActor* Target, const FVector& LaunchVelocity) const;
	bool IsCrunchActivationActive() const { return bCrunchActivationActive; }

	UPROPERTY(EditDefaultsOnly, Category = "Crunch|Cost")
	float CrunchManaCost = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch|Cooldown")
	float CrunchCooldown = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch|Cooldown")
	FGameplayTag CrunchCooldownTag;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch|Damage")
	float DefaultCrunchDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crunch|Damage")
	FGameplayTag DefaultCrunchDamageType;

	/** A bounded movement/cue timer is always owned by the activation that created it. */
	TArray<FTimerHandle> CrunchTimers;

private:
	mutable FGameplayTagContainer CachedCrunchCooldownTags;
	TSet<FString> CrunchHitLedger;
	bool bCrunchActivationActive = false;
};
