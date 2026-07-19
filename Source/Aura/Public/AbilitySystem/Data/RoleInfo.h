// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoleInfo.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UMaterialInstance;
class UNiagaraSystem;
class USoundBase;
class UGameplayEffect;
class UGameplayAbility;

/**
 * Player hero identity ("Role"). A role is identified purely by its name string (FName) as
 * declared in Content/Config/RoleConfig.json — there is no hardcoded C++ enum. Each role
 * selects a whole asset set (mesh, anim blueprint, weapon, sockets, death VFX) and gameplay
 * defaults (primary attributes + startup abilities). Orthogonal to ECharacterClass, which
 * enemies still use for their own attribute/ability sets.
 */
USTRUCT(BlueprintType)
struct FRoleDefaultInfo
{
	GENERATED_BODY()

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
};