// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AuraGameplayTags.h"
#include "AuraAttributeGameplayEffect.generated.h"

/**
 * C++ GE for attribute initialization. Replaces the BP UAssets:
 *   PrimaryAttributes_SetByCaller, SecondaryAttributes, VitalAttributes.
 *
 * Adds SetByCaller modifiers for every attribute in UAuraAttributeSet.
 * At spec creation, AssignTagSetByCallerMagnitude fills in the values
 * from RoleConfig.json / GameplayEffects.json. Unassigned tags default
 * to 0, which is harmless (Additive 0 = no change).
 *
 * For the Infinite-duration variant, see UAuraAttributeGameplayEffect_Infinite.
 *
 * NOTE: The modifier DataTags are sourced from FAuraGameplayTags, whose members
 * are only populated by FAuraGameplayTags::InitializeNativeGameplayTags() (called
 * from UAuraAssetManager::StartInitialLoading). This class's CDO is constructed
 * at module load, *before* that runs, so the constructor would bake DataTag=None
 * and AssignTagSetByCallerMagnitude could never bind (attributes would stay 0).
 * RebuildModifiers() is called from UAuraAssetManager after the tags are
 * initialized to re-bake the modifiers with valid DataTags.
 */
UCLASS()
class AURAABILITYGRAPH_API UAuraAttributeGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UAuraAttributeGameplayEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;
        BuildModifiers();
    }

    /** Re-bake SetByCaller modifier DataTags from FAuraGameplayTags. Call once after
     *  FAuraGameplayTags::InitializeNativeGameplayTags() so the DataTags are valid
     *  (the CDO constructor runs at module load, before the tags are populated). */
    void RebuildModifiers() { BuildModifiers(); }

private:
    void BuildModifiers()
    {
        Modifiers.Reset();

        const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();

        auto AddMod = [this](FGameplayAttribute Attr, FGameplayTag Tag)
        {
            FGameplayModifierInfo& Info = Modifiers.AddDefaulted_GetRef();
            Info.Attribute = Attr;
            Info.ModifierOp = EGameplayModOp::Additive;
            FSetByCallerFloat SetByCaller;
            SetByCaller.DataTag = Tag;
            Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
        };

        // Primary
        AddMod(UAuraAttributeSet::GetStrengthAttribute(),           Tags.Attributes_Primary_Strength);
        AddMod(UAuraAttributeSet::GetIntelligenceAttribute(),       Tags.Attributes_Primary_Intelligence);
        AddMod(UAuraAttributeSet::GetResilienceAttribute(),         Tags.Attributes_Primary_Resilience);
        AddMod(UAuraAttributeSet::GetVigorAttribute(),              Tags.Attributes_Primary_Vigor);
        // Secondary
        AddMod(UAuraAttributeSet::GetArmorAttribute(),              Tags.Attributes_Secondary_Armor);
        AddMod(UAuraAttributeSet::GetArmorPenetrationAttribute(),   Tags.Attributes_Secondary_ArmorPenetration);
        AddMod(UAuraAttributeSet::GetBlockChanceAttribute(),        Tags.Attributes_Secondary_BlockChance);
        AddMod(UAuraAttributeSet::GetCriticalHitChanceAttribute(), Tags.Attributes_Secondary_CriticalHitChance);
        AddMod(UAuraAttributeSet::GetCriticalHitDamageAttribute(),  Tags.Attributes_Secondary_CriticalHitDamage);
        AddMod(UAuraAttributeSet::GetCriticalHitResistanceAttribute(), Tags.Attributes_Secondary_CriticalHitResistance);
        AddMod(UAuraAttributeSet::GetHealthRegenerationAttribute(), Tags.Attributes_Secondary_HealthRegeneration);
        AddMod(UAuraAttributeSet::GetManaRegenerationAttribute(),   Tags.Attributes_Secondary_ManaRegeneration);
        AddMod(UAuraAttributeSet::GetMaxHealthAttribute(),          Tags.Attributes_Secondary_MaxHealth);
        AddMod(UAuraAttributeSet::GetMaxManaAttribute(),           Tags.Attributes_Secondary_MaxMana);
        // Resistance
        AddMod(UAuraAttributeSet::GetFireResistanceAttribute(),      Tags.Attributes_Resistance_Fire);
        AddMod(UAuraAttributeSet::GetLightningResistanceAttribute(), Tags.Attributes_Resistance_Lightning);
        AddMod(UAuraAttributeSet::GetArcaneResistanceAttribute(),    Tags.Attributes_Resistance_Arcane);
        AddMod(UAuraAttributeSet::GetPhysicalResistanceAttribute(),  Tags.Attributes_Resistance_Physical);
    }
};

/**
 * Infinite-duration variant. Same modifiers, DurationPolicy = Infinite.
 * Replaces SecondaryAttributes_Infinite BP UAsset.
 */
UCLASS()
class AURAABILITYGRAPH_API UAuraAttributeGameplayEffect_Infinite : public UAuraAttributeGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraAttributeGameplayEffect_Infinite()
    {
        DurationPolicy = EGameplayEffectDurationType::Infinite;
    }
};

/**
 * C++ GE for pickup/buff effects used by AuraEffectActor's named JSON entries.
 *
 * Adds SetByCaller modifiers for every player-facing attribute so pickup entries
 * can target arbitrary attributes by gameplay tag. Magnitudes come from
 * GameplayEffects.json at runtime.
 *
 * NOTE: Same CDO/DataTag timing caveat as UAuraAttributeGameplayEffect —
 * RebuildModifiers() must be called after native tags are initialized.
 */
UCLASS()
class AURAABILITYGRAPH_API UAuraPickupGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraPickupGameplayEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;
        BuildModifiers();
    }

    /** Re-bake SetByCaller modifier DataTags from FAuraGameplayTags. */
    void RebuildModifiers() { BuildModifiers(); }

private:
    void BuildModifiers()
    {
        Modifiers.Reset();

        const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();

        auto AddMod = [this](FGameplayAttribute Attribute, FGameplayTag DataTag)
        {
            FGameplayModifierInfo& Info = Modifiers.AddDefaulted_GetRef();
            Info.Attribute = Attribute;
            Info.ModifierOp = EGameplayModOp::Additive;
            FSetByCallerFloat SetByCaller;
            SetByCaller.DataTag = DataTag;
            Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
        };

        AddMod(UAuraAttributeSet::GetStrengthAttribute(), Tags.Attributes_Primary_Strength);
        AddMod(UAuraAttributeSet::GetIntelligenceAttribute(), Tags.Attributes_Primary_Intelligence);
        AddMod(UAuraAttributeSet::GetResilienceAttribute(), Tags.Attributes_Primary_Resilience);
        AddMod(UAuraAttributeSet::GetVigorAttribute(), Tags.Attributes_Primary_Vigor);
        AddMod(UAuraAttributeSet::GetArmorAttribute(), Tags.Attributes_Secondary_Armor);
        AddMod(UAuraAttributeSet::GetArmorPenetrationAttribute(), Tags.Attributes_Secondary_ArmorPenetration);
        AddMod(UAuraAttributeSet::GetBlockChanceAttribute(), Tags.Attributes_Secondary_BlockChance);
        AddMod(UAuraAttributeSet::GetCriticalHitChanceAttribute(), Tags.Attributes_Secondary_CriticalHitChance);
        AddMod(UAuraAttributeSet::GetCriticalHitDamageAttribute(), Tags.Attributes_Secondary_CriticalHitDamage);
        AddMod(UAuraAttributeSet::GetCriticalHitResistanceAttribute(), Tags.Attributes_Secondary_CriticalHitResistance);
        AddMod(UAuraAttributeSet::GetHealthRegenerationAttribute(), Tags.Attributes_Secondary_HealthRegeneration);
        AddMod(UAuraAttributeSet::GetManaRegenerationAttribute(), Tags.Attributes_Secondary_ManaRegeneration);
        AddMod(UAuraAttributeSet::GetMaxHealthAttribute(), Tags.Attributes_Secondary_MaxHealth);
        AddMod(UAuraAttributeSet::GetMaxManaAttribute(), Tags.Attributes_Secondary_MaxMana);
        AddMod(UAuraAttributeSet::GetFireResistanceAttribute(), Tags.Attributes_Resistance_Fire);
        AddMod(UAuraAttributeSet::GetLightningResistanceAttribute(), Tags.Attributes_Resistance_Lightning);
        AddMod(UAuraAttributeSet::GetArcaneResistanceAttribute(), Tags.Attributes_Resistance_Arcane);
        AddMod(UAuraAttributeSet::GetPhysicalResistanceAttribute(), Tags.Attributes_Resistance_Physical);
        AddMod(UAuraAttributeSet::GetHealthAttribute(), Tags.Attributes_Vital_Health);
        AddMod(UAuraAttributeSet::GetManaAttribute(), Tags.Attributes_Vital_Mana);
    }
};

/** Duration pickup that executes immediately when periodic. */
UCLASS()
class AURAABILITYGRAPH_API UAuraPickupGameplayEffect_Duration : public UAuraPickupGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraPickupGameplayEffect_Duration()
    {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        bExecutePeriodicEffectOnApplication = true;
    }
};

/** Duration pickup whose first periodic execution waits for one period. */
UCLASS()
class AURAABILITYGRAPH_API UAuraPickupGameplayEffect_DurationDelayed : public UAuraPickupGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraPickupGameplayEffect_DurationDelayed()
    {
        DurationPolicy = EGameplayEffectDurationType::HasDuration;
        bExecutePeriodicEffectOnApplication = false;
    }
};

/** Infinite pickup that executes immediately when periodic. */
UCLASS()
class AURAABILITYGRAPH_API UAuraPickupGameplayEffect_Infinite : public UAuraPickupGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraPickupGameplayEffect_Infinite()
    {
        DurationPolicy = EGameplayEffectDurationType::Infinite;
        bExecutePeriodicEffectOnApplication = true;
    }
};

/** Infinite pickup whose first periodic execution waits for one period. */
UCLASS()
class AURAABILITYGRAPH_API UAuraPickupGameplayEffect_InfiniteDelayed : public UAuraPickupGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraPickupGameplayEffect_InfiniteDelayed()
    {
        DurationPolicy = EGameplayEffectDurationType::Infinite;
        bExecutePeriodicEffectOnApplication = false;
    }
};
