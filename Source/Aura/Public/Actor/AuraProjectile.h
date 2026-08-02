// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AuraAbilityTypes.h"
#include "GameFramework/Actor.h"
#include "AuraProjectile.generated.h"

class UNiagaraSystem;
class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UNiagaraComponent;

UCLASS()
class AURA_API AAuraProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AAuraProjectile();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool ConfigureFromDefinition(FName InDefinitionName);

	UPROPERTY(ReplicatedUsing = OnRep_ProjectileDefinition, BlueprintReadOnly, Category = "Projectile|DataDriven")
	FName ProjectileDefinitionName;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FDamageEffectParams DamageEffectParams;

	UPROPERTY()
	TObjectPtr<USceneComponent> HomingTargetSceneComponent;

	// Public so the AuraAbilityGraph smoke test can invoke it directly when driving a
	// projectile headlessly (the transient test world doesn't run BeginPlay or dispatch
	// OnComponentHit). In-game, MoveComponent fires this delegate on a blocking sweep.
	// Called when the Sphere physically *blocks* a collision (e.g. a wall/static geometry).
	// Overlap (OnSphereOverlap) does not fire against actors that block ECC_Projectile, so a
	// QueryOnly sphere with no Block response simply passes through walls. Blocking WorldStatic
	// here makes the projectile stop and lets us play the impact effect + destroy on contact.
	UFUNCTION()
	virtual void OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(BlueprintPure, Category = "Projectile")
	USphereComponent* GetSphereComponent() const { return Sphere; }

protected:
	virtual void BeginPlay() override;
	virtual void OnDefinitionConfigured(const struct FAuraProjectileDefinition& Definition);

	UFUNCTION(BlueprintCallable)
	virtual void OnHit();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayImpactEffects(const FVector_NetQuantize& ImpactLocation);

	virtual void PlayImpactEffects(const FVector& ImpactLocation);
	virtual void Destroyed() override;

	UFUNCTION()
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// Shared impact resolution: plays the impact effect and, on authority, applies damage to the
	// hit actor (if it has an ASC and is hostile) and destroys the projectile.
	void ApplyImpactAndDestroy(AActor* OtherActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UNiagaraComponent> FlightTrailComponent;

	bool IsValidOverlap(AActor* OtherActor);
	bool bHit = false;

	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;

	void StopLoopingSound();
private:
	UFUNCTION()
	void OnRep_ProjectileDefinition();

	UPROPERTY(EditDefaultsOnly)
	float LifeSpan = 15.f;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> FlightTrail;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere)
	TObjectPtr<USoundBase> LoopingSound;
};
