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
 */
UCLASS()
class AURAABILITYGRAPH_API UAuraAttributeGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UAuraAttributeGameplayEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;

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
 * C++ GE for pickup/buff effects. Replaces the BP UAssets used by AuraEffectActor
 * (InstantGameplayEffectClass, DurationGameplayEffectClass, InfiniteGameplayEffectClass).
 *
 * Adds SetByCaller modifiers for Health and Mana so potions/buffs can modify them.
 * Magnitude values come from GameplayEffects.json at runtime.
 * DurationPolicy is set to Instant by default; the spec can override duration
 * via SetDuration when a Duration or Infinite effect is needed.
 */
UCLASS()
class AURAABILITYGRAPH_API UAuraPickupGameplayEffect : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UAuraPickupGameplayEffect()
    {
        DurationPolicy = EGameplayEffectDurationType::Instant;

        // Health and Mana are the most common pickup effects (potions, heals).
        // Add SetByCaller modifiers so the magnitude can be set per-application.
        const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();

        FGameplayModifierInfo& HealthInfo = Modifiers.AddDefaulted_GetRef();
        HealthInfo.Attribute = UAuraAttributeSet::GetHealthAttribute();
        HealthInfo.ModifierOp = EGameplayModOp::Additive;
        FSetByCallerFloat HealthSetByCaller;
        HealthSetByCaller.DataTag = Tags.Attributes_Vital_Health;
        HealthInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(HealthSetByCaller);

        FGameplayModifierInfo& ManaInfo = Modifiers.AddDefaulted_GetRef();
        ManaInfo.Attribute = UAuraAttributeSet::GetManaAttribute();
        ManaInfo.ModifierOp = EGameplayModOp::Additive;
        FSetByCallerFloat ManaSetByCaller;
        ManaSetByCaller.DataTag = Tags.Attributes_Vital_Mana;
        ManaInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(ManaSetByCaller);
    }
};