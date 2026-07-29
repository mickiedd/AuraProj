// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AuraDamageGameplayEffect.generated.h"

/**
 * C++ GameplayEffect for damage application. Replaces the legacy GE_Damage Blueprint UAsset.
 *
 * The damage pipeline is fully data-driven:
 *   - Base damage and damage type tag are set on the spec via SetByCaller at runtime
 *     (ApplyDamageEffect / CauseDamageNode), read from UAuraAbilityDefinition (XML).
 *   - UExecCalc_Damage reads SetByCaller magnitudes by tag, applies resistances,
 *     block chance, armor penetration, critical hits, and debuffs — all in C++.
 *   - No UAsset, no Blueprint, no per-damage-type modifier config needed.
 *
 * Mirrors the pattern already used by UAuraManaCostGameplayEffect and
 * UAuraCooldownGameplayEffect (cost and cooldown are also pure C++ GEs).
 */
UCLASS()
class AURAABILITYGRAPH_API UAuraDamageGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UAuraDamageGameplayEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;
        FGameplayEffectExecutionDefinition& ExecDef = Executions.AddDefaulted_GetRef();
        ExecDef.CalculationClass = UExecCalc_Damage::StaticClass();
    }
};