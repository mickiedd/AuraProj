// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilityGraphTypes.h"
#include "AuraDamageGameplayEffect.h"
#include "AbilityDefinition.generated.h"

class UAuraAbilityActionNode;
class UAuraDataAbility;
struct FDamageEffectParams;

/**
 * Runtime ability definition loaded from an XML file on disk.
 * Always created as a transient object (GetTransientPackage()) - never saved as a UAsset.
 * Populated entirely by LoadFromXML(). EditDefaultsOnly UPROPERTYs intentionally absent:
 * this class has no editor presence.
 */
UCLASS(Transient)
class AURAABILITYGRAPH_API UAuraAbilityDefinition : public UObject
{
    GENERATED_BODY()

public:
    // Identity — set from <ability> attributes
    FName AbilityName;
    FGameplayTag AbilityTag;
    FGameplayTagContainer AbilityTags;
    FGameplayTag InputTag;
    FGameplayTag AbilityType;

    // Firearm contract — present only on the active FireGun definition.
    FString FireMode = TEXT("Unspecified");
    float MinimumShotInterval = 0.f;
    int32 MagazineCapacity = 0;
    int32 ReserveCapacity = 0;
    int32 ShotConsumption = 0;
    float ReloadDuration = 0.f;
    bool bHasFirearmContract = false;

    // Cost / cooldown — set from <cost> / <cooldown>
    float ManaCost = 0.f;
    FGameplayTag CooldownTag;
    FScalableFloat CooldownDuration;

    // Damage — set from <damage>. Defaults to UAuraDamageGameplayEffect (pure C++, no UAsset).
    // If <damage effectClass="..."> is present in XML, it overrides this with a custom GE class.
    TSubclassOf<UGameplayEffect> DamageEffectClass = UAuraDamageGameplayEffect::StaticClass();
    FGameplayTag DamageType;
    FScalableFloat Damage;
    float DebuffChance = 20.f;
    float DebuffDamage = 5.f;
    float DebuffDuration = 5.f;
    float DebuffFrequency = 1.f;
    float DeathImpulseMagnitude = 10000.f;
    float KnockbackForceMagnitude = 10000.f;
    float KnockbackChance = 0.f;

    // Action graph — root node built from <graph> during LoadFromXML
    UPROPERTY(Instanced)
    TObjectPtr<UAuraAbilityActionNode> RootNode;

    // Cached source XML for debugging / log output
    FString SourceXML;

    // Original XML path used by the editor import factory for reimporting.
    UPROPERTY()
    FString SourceFilePath;

    /** Parse XML content and populate all fields. Called by LoadAbilityDefinitionFromXMLFile. */
    bool LoadFromXML(const FString& XMLContent);

    bool IsFirearmDefinition() const { return bHasFirearmContract; }

    /**
     * Builds a fully-populated FDamageEffectParams from this definition's damage
     * data (type/base, debuffs, death impulse, knockback). All seven damage-producing
     * nodes (ApplyDamage, CauseDamage, HitscanTrace, SpawnProjectile, SpawnProjectiles,
     * ElectrocuteBeam, SpawnShards) route through this so the direction-based
     * impulse/knockback contract lives in one place and is directly testable.
     */
    void BuildDamageEffectParams(FDamageEffectParams& OutParams, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, AActor* WorldContext, float AbilityLevel, const FVector& Direction, bool bIsRadialDamage = false, const FVector& RadialOrigin = FVector::ZeroVector, float RadialInnerRadius = 0.f, float RadialOuterRadius = 0.f) const;
};
