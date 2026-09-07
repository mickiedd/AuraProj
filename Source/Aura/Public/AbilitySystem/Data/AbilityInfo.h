// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "AbilityInfo.generated.h"

class UGameplayAbility;

/**
 * UI metadata for a single ability. Populated either from Content/Config/AbilityInfo.json
 * (via URuntimeAbilityInfo) or from the legacy DA_AbilityInfo UAsset (via UAbilityInfo).
 */
USTRUCT(BlueprintType)
struct FAuraAbilityInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag AbilityTag = FGameplayTag();

	// UI-only fields (loaded from AbilityInfo.json / DA_AbilityInfo)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UTexture2D> Icon = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<const UMaterialInterface> BackgroundMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 LevelRequirement = 1;

	// Runtime fields (resolved from AbilityDefinition XML or ASC at query time)
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag InputTag = FGameplayTag();

	UPROPERTY(BlueprintReadOnly)
	FGameplayTag StatusTag = FGameplayTag();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag CooldownTag = FGameplayTag();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AbilityType = FGameplayTag();

	/**
	 * LEGACY: Ability class, used by UAbilityInfo (DA_AbilityInfo UAsset) during migration.
	 * Not populated by URuntimeAbilityInfo (JSON path). Data-driven abilities use
	 * UAuraDataAbility granted via RoleConfig.json instead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> Ability;
};

/**
 * Runtime ability info registry loaded from Content/Config/AbilityInfo.json.
 * Replaces the old DA_AbilityInfo UAsset. Held as a transient object (never saved/cooked).
 * Server: one instance per GameMode. Client: process-lifetime static cache.
 */
UCLASS(Transient)
class AURA_API URuntimeAbilityInfo : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Parse AbilityInfo.json and populate the ability info map.
	 * Expected JSON schema:
	 * {
	 *   "abilities": [
	 *     {
	 *       "abilityTag": "Abilities.Fire.FireBolt",
	 *       "icon": "/Game/UI/Icons/T_FireBolt.T_FireBolt",
	 *       "backgroundMaterial": "/Game/UI/Materials/M_AbilityBackground_Fire.M_AbilityBackground_Fire",
	 *       "levelRequirement": 1
	 *     }
	 *   ]
	 * }
	 */
	bool LoadFromJSON(const FString& JSONContent);

	/** Stable, tag-sorted UI metadata used by status/grant iteration. */
	TArray<FAuraAbilityInfo> GetAllAbilityInfo() const;

	/**
	 * Look up UI metadata for an ability by its tag. Returns empty struct if not found.
	 * Note: InputTag, StatusTag, CooldownTag, AbilityType are NOT stored in JSON - they
	 * must be resolved from the ability definition XML or the ASC at query time.
	 */
	FAuraAbilityInfo FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound = false) const;

private:
	/** Keep JSON-resolved UI assets visible to GC for the lifetime of this rooted registry. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FAuraAbilityInfo> AbilityInfoMap;
};

/**
 * DEPRECATED: Legacy UAsset-based ability info. Kept for backward compatibility during migration.
 * New code should use URuntimeAbilityInfo (loaded from JSON) instead.
 */
UCLASS()
class AURA_API UAbilityInfo : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AbilityInformation")
	TArray<FAuraAbilityInfo> AbilityInformation;

	FAuraAbilityInfo FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound = false) const;
};
