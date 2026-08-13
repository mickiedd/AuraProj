// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RoleInfo.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UMaterialInstance;
class UNiagaraSystem;
class USoundBase;
class UGameplayEffect;
class UGameplayAbility;
class UObject;

UENUM()
enum class EAuraRoleValidationSeverity : uint8
{
	Warning,
	Error
};

USTRUCT()
struct AURA_API FAuraRoleValidationIssue
{
	GENERATED_BODY()

	UPROPERTY()
	EAuraRoleValidationSeverity Severity = EAuraRoleValidationSeverity::Error;

	UPROPERTY()
	FName RoleId = NAME_None;

	UPROPERTY()
	FString JsonPath;

	UPROPERTY()
	FString Message;
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct FRoleDefaultInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag EntityType;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag ControlType;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag CombatProfile;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag Faction;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag DeathPolicy;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag EconomyProfile;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	FGameplayTag InteractionProfile;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	bool bPlayerSelectable = false;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	bool bTargetable = false;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	bool bCanAttack = false;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	bool bCanBeDamaged = false;

	UPROPERTY(VisibleAnywhere, Category = "Role|Identity")
	bool bAllowFriendlyFire = false;

	/* Visuals */

	UPROPERTY(EditDefaultsOnly, Category = "Role|Visuals")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Visuals")
	TSubclassOf<UAnimInstance> AnimBlueprintClass;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Visuals")
	TObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Visuals")
	FName WeaponSocketName = FName("WeaponHandSocket");

	/* Combat sockets (skeleton-dependent). Copied onto the character at apply time
	   so GetCombatSocketLocation returns sockets that exist on this role's skeleton. */

	UPROPERTY(EditDefaultsOnly, Category = "Role|Combat")
	FName WeaponTipSocketName;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Combat")
	FName LeftHandSocketName;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Combat")
	FName RightHandSocketName;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Combat")
	FName TailSocketName;

	/* Death / dissolve VFX */

	UPROPERTY(EditDefaultsOnly, Category = "Role|Death")
	TObjectPtr<UMaterialInstance> DissolveMaterialInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Death")
	TObjectPtr<UMaterialInstance> WeaponDissolveMaterialInstance;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Death")
	TObjectPtr<UNiagaraSystem> BloodEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Death")
	TObjectPtr<USoundBase> DeathSound;

	/* Gameplay (per role). Primary attribute values applied at init via the shared
	   PrimaryAttributes_SetByCaller GE (see UCharacterClassInfo). Secondary/Vital stay shared. */

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	float Strength = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	float Intelligence = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	float Resilience = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	float Vigor = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	TArray<TSubclassOf<UGameplayAbility>> StartupPassiveAbilities;

	/** Legacy abilities that may be unlocked by level/cheats but are not granted at startup. */
	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	TArray<TSubclassOf<UGameplayAbility>> UnlockableAbilities;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> StartupAbilityDefinitions;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> StartupPassiveAbilityDefinitions;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	TArray<FString> StartupAbilityDefinitionPaths;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	TArray<FString> StartupPassiveAbilityDefinitionPaths;

	UPROPERTY(Transient)
	TObjectPtr<UObject> DefaultLMBAbilityDefinition;

	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	FString DefaultLMBAbilityDefinitionPath;

	/* The LMB default skill for this role (the ability bound to InputTag.LMB at startup).
	   Data-driven: when set, ApplyRole grants this ability as the role's LMB skill and equips
	   the role's weapon; when empty, the role gets NO LMB skill and NO weapon (any BP-default
	   LMB-tagged startup ability is stripped). This is what distinguishes a weaponless role
	   (e.g. BungeeMan) from Aura, whose LMB skill is GA_FireBolt. */
	UPROPERTY(EditDefaultsOnly, Category = "Role|Gameplay")
	TSubclassOf<UGameplayAbility> DefaultLMBAbility;
};

/**
 * Data asset holding the per-role configuration, built at runtime from
 * Content/Config/RoleConfig.json. Roles are keyed by their name (FName); there is no
 * hardcoded role enum. Accessed via UAuraAbilitySystemLibrary::GetRoleInfo / GetDefaultRole.
 */
UCLASS()
class AURA_API URoleInfo : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "Role Defaults")
	int32 RoleDefinitionVersion = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Role Defaults")
	TMap<FName, FRoleDefaultInfo> RoleInformation;

	/** Role used when no role is explicitly chosen (no save slot / no UI selection).
	    Set from the top-level "defaultRole" field in RoleConfig.json. */
	UPROPERTY(VisibleAnywhere, Category = "Role Defaults")
	FName DefaultRole = NAME_None;

	/** All role names known to this config (the map keys), for UI / iteration. */
	TArray<FName> GetRoleNames() const;

	FRoleDefaultInfo GetRoleDefaultInfo(FName Role) const;

	/** True only if the role exists in the map AND has both a SkeletalMesh and an
	 *  AnimBlueprintClass loaded. Used to reject login for under-configured roles
	 *  (e.g. a placeholder role with empty mesh/anim). */
	bool IsRoleConfigured(FName Role) const;

	/** Server/login-safe predicate. Presentation callers may inspect non-player roles separately. */
	bool IsPlayerRoleSelectable(FName Role) const;
};

USTRUCT()
struct AURA_API FAuraRoleLoadResult
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<URoleInfo> Candidate;

	UPROPERTY()
	int32 DetectedVersion = 0;

	UPROPERTY()
	int32 PublishedVersion = 2;

	UPROPERTY()
	TArray<FAuraRoleValidationIssue> Issues;

	UPROPERTY()
	bool bCanPublish = false;

	FString ToLogString() const;
};
