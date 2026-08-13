// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilityGraphTypes.h"
#include "AuraManaCostGameplayEffect.h"
#include "DataAbility.generated.h"

class UAuraAbilityDefinition;
class UAuraAbilityActionTask;
class UAbilityTask;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UTargetDataUnderMouse;

UCLASS(meta=(NotBlueprintable))
class AURAABILITYGRAPH_API UAuraDataAbility : public UAuraGameplayAbility
{
    GENERATED_BODY()

public:
    /**
     * Shared C++ GE class used for mana cost (SetByCaller magnitude).
     * Hardcoded to UGameplayEffect in the constructor — not designer-overridable.
     * Stored as a field (not UPROPERTY) because no editor exposure is wanted.
     */
    TSubclassOf<UGameplayEffect> SharedCostGE;

    /**
     * Shared C++ GE class used for cooldown (HasDuration, SetByCaller duration).
     * Hardcoded to UAuraCooldownGameplayEffect in the constructor — not designer-overridable.
     */
    TSubclassOf<UGameplayEffect> SharedCooldownGE;

    UAuraDataAbility();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
    virtual const FGameplayTagContainer* GetCooldownTags() const override;

    const UAuraAbilityDefinition* GetDefinition() const;

    /**
     * Authority-only graph actions may complete locally on a predicting client
     * without producing their server-side effect. That client must wait for the
     * authoritative instance to end the activation.
     */
    static bool ShouldEndAfterGraphCompletion(bool bIsNetAuthority)
    {
        return bIsNetAuthority;
    }

    void AdvanceGraph(EAuraAbilityActionStatus ChildStatus);

    UFUNCTION(BlueprintCallable, Category = "Ability|TargetData")
    void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& DataHandle);

    UFUNCTION(BlueprintCallable, Category = "Ability|Montage")
    void OnMontageEventReceived(FGameplayEventData EventData);

    UFUNCTION(BlueprintCallable, Category = "Ability|Montage")
    void OnMontageCompleted();

    UFUNCTION(BlueprintCallable, Category = "Ability|Montage")
    void OnMontageInterrupted();

protected:
    // UPROPERTY: keeps the per-activation task tree rooted on the (instanced) ability
    // so it can't be GC'd mid-channel, and is released cleanly when EndAbility nulls it.
    UPROPERTY()
    UAuraAbilityActionTask* RootTask = nullptr;
    bool bGraphActive = false;
    bool bWaitingForAuthoritativeEnd = false;

    // Re-entrancy guard: ending a Pending montage/event task can fire OnInterrupted
    // synchronously, which would re-enter EndAbility via AdvanceGraph(Failure). The
    // guard makes the recursive call a no-op.
    bool bIsEndingAbility = false;

    mutable FGameplayTagContainer CachedCooldownTags;

    FAuraAbilityExecutionContext PersistentCtx;

public:
    TWeakObjectPtr<UTargetDataUnderMouse> PendingTargetDataTask;
    TWeakObjectPtr<UAbilityTask_PlayMontageAndWait> PendingMontageTask;
    TWeakObjectPtr<UAbilityTask_WaitGameplayEvent> PendingMontageEventTask;
};

/** Graph ability class discoverable by existing enemy BT tasks through Abilities.Attack. */
UCLASS(meta=(NotBlueprintable))
class AURAABILITYGRAPH_API UAuraEnemyAttackDataAbility : public UAuraDataAbility
{
    GENERATED_BODY()
public:
    UAuraEnemyAttackDataAbility();
};

/** Graph ability class activated by AuraAttributeSet through Effects.HitReact. */
UCLASS(meta=(NotBlueprintable))
class AURAABILITYGRAPH_API UAuraEnemyHitReactDataAbility : public UAuraDataAbility
{
    GENERATED_BODY()
public:
    UAuraEnemyHitReactDataAbility();
};
