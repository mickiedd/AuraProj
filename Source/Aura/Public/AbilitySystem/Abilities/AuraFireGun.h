// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AuraProjectileSpell.h"
#include "AuraFireGun.generated.h"

class UParticleSystem;
class USoundBase;
struct FHitResult;

/**
 * This is a non-active compatibility helper for BungeeMan's LMB gun skill. The active runtime definition is
 * `Content/AbilityDefinitions/FireGun.xml`; this class is retained only for compatibility with
 * already-authored packages and is not granted or selected by RoleConfig. It fires a real bullet
 * projectile (AAuraBullet) from the gun muzzle
 * toward the cursor target. The bullet travels fast + straight, deals damage on overlap via the
 * shared AuraDamageGameplayAbility damage params, and plays its own impact FX. Cooldown-only
 * (no Mana cost); fire rate is gated by the Cooldown.Gun.Fire GameplayEffect.
 *
 * Ported from Blaster's gun concept but expressed as an Aura GameplayAbility: extends
 * UAuraProjectileSpell to reuse SpawnProjectile + ProjectileClass (set to BP_AuraBullet on the
 * ability Blueprint). The muzzle flash + fire sound are relayed to all clients via
 * AAuraCharacterBase::MulticastPlayGunFireFX.
 *
 * Flow (mirrors GA_FireBolt):
 *   - The Blueprint gathers the cursor target via the TargetDataUnderMouse ability task and plays
 *     a fire montage with an AN_MontageEvent notify tagged "Event.Montage.FireGun".
 *   - On that event the BP calls FireGun(CursorHitResult) with the cached cursor FHitResult.
 *   - Server spawns the bullet (SpawnProjectile) toward the cursor impact point and multicasts
 *     the muzzle FX. Non-authority clients no-op (they see the bullet via replication + muzzle via
 *     the multicast).
 *
 * The muzzle is the role's weapon-tip socket (CombatSocket.Weapon), so the gun's barrel must
 * expose a socket named in RoleConfig.json's per-role "weaponTipSocket".
 */
UCLASS()
class AURA_API UAuraFireGun : public UAuraProjectileSpell
{
	GENERATED_BODY()

public:
	/** Spawns a bullet from the muzzle toward the cursor impact point (server-only) and
	 *  multicasts the muzzle flash + fire sound. Called from the BP on the
	 *  "Event.Montage.FireGun" gameplay event. */
	UFUNCTION(BlueprintCallable, Category = "FireGun")
	void FireGun(const FHitResult& CursorHitResult);

protected:
	/* Muzzle flash + fire sound, played on all clients via the avatar's NetMulticast. The bullet
	 * handles its own impact/tracer FX, so only muzzle cosmetics live on the ability. */
	UPROPERTY(EditDefaultsOnly, Category = "FireGun|FX")
	TObjectPtr<UParticleSystem> MuzzleEffect;

	UPROPERTY(EditDefaultsOnly, Category = "FireGun|FX")
	TObjectPtr<USoundBase> FireSound;
};
