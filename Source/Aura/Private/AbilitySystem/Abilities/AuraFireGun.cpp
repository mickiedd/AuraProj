// Copyright Druid Mechanics


#include "AbilitySystem/Abilities/AuraFireGun.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Aura/AuraLogChannels.h"
#include "AuraGameplayTags.h"
#include "Character/AuraCharacterBase.h"
#include "Interaction/CombatInterface.h"

void UAuraFireGun::FireGun(const FHitResult& CursorHitResult)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority())
	{
		// Only the server spawns the bullet; clients see it via AAuraProjectile replication and
		// the muzzle FX via the multicast below.
		return;
	}

	const FGameplayTag WeaponSocketTag = FAuraGameplayTags::Get().CombatSocket_Weapon;
	const FVector MuzzleLocation = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, WeaponSocketTag);

	// Spawn the bullet (ProjectileClass, set on the ability BP to BP_AuraBullet) from the muzzle
	// toward the cursor impact point. SpawnProjectile is server-only and wires DamageEffectParams
	// from this ability's defaults (Damage/DamageType/DamageEffectClass) onto the bullet.
	SpawnProjectile(CursorHitResult.ImpactPoint, WeaponSocketTag);

	UE_LOG(LogAura, Log, TEXT("[FireGun] Spawned bullet from %s toward %s."),
		*MuzzleLocation.ToString(), *CursorHitResult.ImpactPoint.ToString());

	// Muzzle flash + fire sound on all clients. The bullet handles impact + tracer FX itself.
	if (AAuraCharacterBase* AuraCharacter = Cast<AAuraCharacterBase>(Avatar))
	{
		AuraCharacter->MulticastPlayGunFireFX(MuzzleLocation, MuzzleEffect, FireSound);
	}
}