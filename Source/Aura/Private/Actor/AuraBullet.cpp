// Copyright Druid Mechanics


#include "Actor/AuraBullet.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"

AAuraBullet::AAuraBullet()
{
	TracerMesh = CreateDefaultSubobject<UStaticMeshComponent>("TracerMesh");
	TracerMesh->SetupAttachment(GetRootComponent()); // root is the inherited Sphere collision
	TracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TracerMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	TracerMesh->SetCastShadow(false);

	// Fast, straight, non-homing bullet — no gravity. Tuned to feel near-instant at typical
	// engagement ranges without overlap-tunneling through character-sized targets.
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = 15000.f;
		ProjectileMovement->MaxSpeed = 15000.f;
		ProjectileMovement->ProjectileGravityScale = 0.f;
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->bRotationFollowsVelocity = true;
	}
}

void AAuraBullet::BeginPlay()
{
	Super::BeginPlay();
	// Bullets fly fast; if they miss they should clean up quickly rather than living the full
	// 15s inherited lifespan.
	SetLifeSpan(3.f);
}

void AAuraBullet::PlayImpactEffects(const FVector& ImpactLocation)
{
	if (BulletImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BulletImpactSound, ImpactLocation, GetActorRotation());
	}
	if (BulletImpactParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, BulletImpactParticle, ImpactLocation, GetActorRotation());
	}
	StopLoopingSound();
}