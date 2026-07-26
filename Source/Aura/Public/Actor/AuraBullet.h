// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Actor/AuraProjectile.h"
#include "AuraBullet.generated.h"

class UStaticMeshComponent;
class UParticleSystem;
class USoundBase;

/**
 * A real bullet projectile for BungeeMan's gun LMB skill. Subclasses AAuraProjectile so it
 * reuses the replicated damage-on-overlap + lifespan + impact-FX-multicast machinery, and adds:
 *   - a visible tracer StaticMesh (e.g. MilitaryWeapDark St_Tracer_A) attached to the collision sphere,
 *   - fast, straight, non-homing flight (no gravity) — unlike FireBolt's slow homing bolts,
 *   - Cascade (UParticleSystem) impact FX override, so the MilitaryWeapDark P_Impact_* particles
 *     can be used directly instead of Niagara.
 *
 * Configured via a Blueprint child (BP_AuraBullet): set the tracer mesh + material, the impact
 * particle, and the impact sound. Damage values come from the spawning ability's
 * MakeDamageEffectParamsFromClassDefaults (set on DamageEffectParams before FinishSpawning).
 */
UCLASS()
class AURA_API AAuraBullet : public AAuraProjectile
{
	GENERATED_BODY()

public:
	AAuraBullet();
	virtual void BeginPlay() override;

	// Overrides the base Niagara impact with a Cascade particle + sound (MilitaryWeapDark FX).
	virtual void PlayImpactEffects(const FVector& ImpactLocation) override;

protected:
	/** Visible tracer round. NoCollision so the Sphere overlap drives hits, not the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet")
	TObjectPtr<UStaticMeshComponent> TracerMesh;

	/** Cascade impact particle (e.g. P_Impact_Stone_Medium_01). */
	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<UParticleSystem> BulletImpactParticle;

	/** Impact sound played at the hit location. */
	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<USoundBase> BulletImpactSound;
};