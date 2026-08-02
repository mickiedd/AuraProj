// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Actor/AuraProjectile.h"
#include "AuraFireBall.generated.h"

/**
 * 
 */
UCLASS()
class AURA_API AAuraFireBall : public AAuraProjectile
{
	GENERATED_BODY()
public:
	AAuraFireBall();
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_ReturnToActor, BlueprintReadOnly)
	TObjectPtr<AActor> ReturnToActor;

	UPROPERTY(BlueprintReadWrite)
	FDamageEffectParams ExplosionDamageParams;
	
protected:
	virtual void BeginPlay() override;
	virtual void OnDefinitionConfigured(const struct FAuraProjectileDefinition& Definition) override;
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	virtual void OnHit() override;
	virtual void PlayImpactEffects(const FVector& ImpactLocation) override;

	UFUNCTION()
	void OnRep_ReturnToActor();

private:
	void TryStartNativeMovement();
	void FinishReturn();

	enum class EFireBallMovementState : uint8 { Waiting, Outgoing, Returning, Finished };
	EFireBallMovementState MovementState = EFireBallMovementState::Waiting;
	FVector OutgoingStart = FVector::ZeroVector;
	FVector OutgoingEnd = FVector::ZeroVector;
	float MovementElapsed = 0.f;
	float OutboundDistance = 800.f;
	float OutboundDuration = 1.f;
	float ReturnSpeed = 800.f;
	float ReturnDistance = 150.f;
	TSet<TWeakObjectPtr<AActor>> DamagedActors;
};
