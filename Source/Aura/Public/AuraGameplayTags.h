// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * AuraGameplayTags
 *
 * Singleton containing native Gameplay Tags
 */

struct FAuraGameplayTags
{
public:
    static const FAuraGameplayTags& Get() { return GameplayTags;}
    static void InitializeNativeGameplayTags();

	FGameplayTag Faction_Player;
	FGameplayTag Faction_Enemy;
	FGameplayTag Faction_Civilian;

	FGameplayTag Entity_Player;
	FGameplayTag Entity_AmbientNPC;

	FGameplayTag Control_Player;
	FGameplayTag Control_EnemyAI;
	FGameplayTag Control_CivilianAI;

	FGameplayTag Combat_Unassigned;
	FGameplayTag Combat_Magic;
	FGameplayTag Combat_Gun;
	FGameplayTag Combat_Melee;
	FGameplayTag Combat_Civilian;

	FGameplayTag Death_PlayerRespawn;
	FGameplayTag Death_EnemyLoot;
	FGameplayTag Death_PopulationRespawn;

	FGameplayTag Economy_None;
	FGameplayTag Economy_Ambient;
	FGameplayTag Economy_CommerceCapable;

	FGameplayTag Interaction_Combatant;
	FGameplayTag Interaction_Civilian;
	FGameplayTag Interaction_Talk;
	FGameplayTag Interaction_Observe;
	FGameplayTag Interaction_Trade;
	FGameplayTag Interaction_UseShelter;

	FGameplayTag Target_Relationship_Hostile;
	FGameplayTag Target_Relationship_Friendly;
	FGameplayTag Target_Relationship_Neutral;
	FGameplayTag Target_Relationship_Protected;
	FGameplayTag Target_Kind_Player;
	FGameplayTag Target_Kind_Enemy;
	FGameplayTag Target_Kind_Civilian;
	FGameplayTag Target_Kind_Shelter;
	FGameplayTag Target_Kind_World;
	FGameplayTag Target_Life_Alive;
	FGameplayTag Target_Life_Dying;
	FGameplayTag Target_Life_Dead;
	FGameplayTag Target_Life_Respawning;

	/** Replicated dynamic source tag attached to every role-owned ability spec. */
	FGameplayTag GrantSource_Role;

	FGameplayTag Attributes_Primary_Strength;
	FGameplayTag Attributes_Primary_Intelligence;
	FGameplayTag Attributes_Primary_Resilience;
	FGameplayTag Attributes_Primary_Vigor;

	FGameplayTag Attributes_Secondary_Armor;
	FGameplayTag Attributes_Secondary_ArmorPenetration;
	FGameplayTag Attributes_Secondary_BlockChance;
	FGameplayTag Attributes_Secondary_CriticalHitChance;
	FGameplayTag Attributes_Secondary_CriticalHitDamage;
	FGameplayTag Attributes_Secondary_CriticalHitResistance;
	FGameplayTag Attributes_Secondary_HealthRegeneration;
	FGameplayTag Attributes_Secondary_ManaRegeneration;
	FGameplayTag Attributes_Secondary_MaxHealth;
	FGameplayTag Attributes_Secondary_MaxMana;

	// Vital (direct Health/Mana modification, used by pickup GEs)
	FGameplayTag Attributes_Vital_Health;
	FGameplayTag Attributes_Vital_Mana;

	FGameplayTag Attributes_Meta_IncomingXP;

	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_1;
	FGameplayTag InputTag_2;
	FGameplayTag InputTag_3;
	FGameplayTag InputTag_4;
	FGameplayTag InputTag_Passive_1;
	FGameplayTag InputTag_Passive_2;
	FGameplayTag InputTag_Interact;

	FGameplayTag Damage;
	FGameplayTag Damage_Fire;
	FGameplayTag Damage_Lightning;
	FGameplayTag Damage_Arcane;
	FGameplayTag Damage_Physical;

	FGameplayTag Attributes_Resistance_Fire;
	FGameplayTag Attributes_Resistance_Lightning;
	FGameplayTag Attributes_Resistance_Arcane;
	FGameplayTag Attributes_Resistance_Physical;

	FGameplayTag Debuff_Burn;
	FGameplayTag Debuff_Stun;
	FGameplayTag Debuff_Arcane;
	FGameplayTag Debuff_Physical;

	FGameplayTag Debuff_Chance;
	FGameplayTag Debuff_Damage;
	FGameplayTag Debuff_Duration;
	FGameplayTag Debuff_Frequency;

	FGameplayTag Abilities_None;
	
	FGameplayTag Abilities_Attack;
	FGameplayTag Abilities_Summon;
	
	FGameplayTag Abilities_HitReact;

	FGameplayTag Abilities_Status_Locked;
	FGameplayTag Abilities_Status_Eligible;
	FGameplayTag Abilities_Status_Unlocked;
	FGameplayTag Abilities_Status_Equipped;

	FGameplayTag Abilities_Cost_Mana;
	FGameplayTag Abilities_Cooldown_Duration;

	FGameplayTag Abilities_Type_Offensive;
	FGameplayTag Abilities_Type_Passive;
	FGameplayTag Abilities_Type_None;
	
	FGameplayTag Abilities_Fire_FireBolt;
	FGameplayTag Abilities_Fire_FireBlast;
	FGameplayTag Abilities_Lightning_Electrocute;
	FGameplayTag Abilities_Arcane_ArcaneShards;
	FGameplayTag Abilities_Gun_Fire;
	FGameplayTag Abilities_Melee_CrunchUppercut;
	FGameplayTag Abilities_Melee_CrunchDash;
	FGameplayTag Abilities_Melee_CrunchGroundBlast;
	FGameplayTag Abilities_Melee_CrunchTornado;


	FGameplayTag Abilities_Passive_HaloOfProtection;
	FGameplayTag Abilities_Passive_LifeSiphon;
	FGameplayTag Abilities_Passive_ManaSiphon;

	FGameplayTag Cooldown_Fire_FireBolt;
	FGameplayTag Cooldown_Gun_Fire;
	FGameplayTag Cooldown_Melee_CrunchUppercut;
	FGameplayTag Cooldown_Melee_CrunchDash;
	FGameplayTag Cooldown_Melee_CrunchGroundBlast;
	FGameplayTag Cooldown_Melee_CrunchTornado;

	FGameplayTag CombatSocket_Weapon;
	FGameplayTag CombatSocket_RightHand;
	FGameplayTag CombatSocket_LeftHand;
	FGameplayTag CombatSocket_Tail;

	FGameplayTag Montage_Attack_1;
	FGameplayTag Montage_Attack_2;
	FGameplayTag Montage_Attack_3;
	FGameplayTag Montage_Attack_4;
	
	TMap<FGameplayTag, FGameplayTag> DamageTypesToResistances;
	TMap<FGameplayTag, FGameplayTag> DamageTypesToDebuffs;

	FGameplayTag Effects_HitReact;

	FGameplayTag Player_Block_InputPressed;
	FGameplayTag Player_Block_InputHeld;
	FGameplayTag Player_Block_InputReleased;
	FGameplayTag Player_Block_CursorTrace;
	FGameplayTag Player_Mounted_Broom;

	FGameplayTag GameplayCue_FireBlast;
	FGameplayTag GameplayCue_MeleeImpact;
	FGameplayTag GameplayCue_CrunchGroundBlast;

private:
    static AURA_API FAuraGameplayTags GameplayTags;
};
