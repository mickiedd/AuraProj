// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Actor/AuraProjectile.h"
#include "AuraBullet.generated.h"

class UStaticMeshComponent;
class UParticleSystem;
class UParticleSystemComponent;
class USoundBase;
class UMaterialInterface;

/**
 * A real bullet projectile for BungeeMan's gun LMB skill. Subclasses AAuraProjectile so it
 * reuses the replicated damage-on-overlap + lifespan + impact-FX-multicast machinery, and adds:
 *   - a visible tracer StaticMesh (e.g. MilitaryWeapDark St_Tracer_A) attached to the collision sphere,
 *   - fast, straight, non-homing flight (no gravity) — unlike FireBolt's slow homing bolts,
 *   - Cascade (UParticleSystem) impact FX override, so the MilitaryWeapDark P_Impact_* particles
 *     can be used directly instead of Niagara.
 *
 * Presentation assets are loaded from the projectile definition; a Blueprint child can still
 * override the exposed properties. Damage values come from the spawning ability's
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
	virtual void PlayImpactEffectsAtSurface(const FVector& ImpactLocation, const FVector& ImpactNormal) override;

protected:
	virtual void OnDefinitionConfigured(const struct FAuraProjectileDefinition& Definition) override;
	virtual void OnHit() override;
	virtual void OnHitAtSurface(const FVector& ImpactLocation, const FVector& ImpactNormal) override;
	void StartFlightEffects();

	/** Visible tracer round. NoCollision so the Sphere overlap drives hits, not the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bullet")
	TObjectPtr<UStaticMeshComponent> TracerMesh;

	/** Cascade impact particle (e.g. P_Impact_Stone_Medium_01). */
	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<UParticleSystem> BulletImpactParticle;

	/** Impact sound played at the hit location. */
	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<USoundBase> BulletImpactSound;

	/** Optional attached Cascade tracer/flight streak. */
	UPROPERTY(Transient)
	TObjectPtr<UParticleSystemComponent> FlightParticleComponent;

	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<UParticleSystem> FlightParticle;

	/** Deferred decal material used for a short-lived wall hit mark. */
	UPROPERTY(EditAnywhere, Category = "Bullet")
	TObjectPtr<UMaterialInterface> BulletHoleMaterial;

	float BulletHoleSize = 8.f;
	float BulletHoleLifeSpan = 30.f;
};
