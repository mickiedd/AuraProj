// Copyright Druid Mechanics


#include "Actor/AuraBullet.h"

#include "Aura/AuraLogChannels.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/AuraGameplayConfig.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
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
	StartFlightEffects();
	// Definition-backed bullets retain their authored lifetime/range. Only legacy,
	// unconfigured bullets use the old short cleanup timer.
	if (ProjectileDefinitionName.IsNone()) SetLifeSpan(3.f);
}

void AAuraBullet::StartFlightEffects()
{
	if (!bHit && GetNetMode() != NM_DedicatedServer && FlightParticle && !IsValid(FlightParticleComponent))
	{
		FlightParticleComponent = UGameplayStatics::SpawnEmitterAttached(
			FlightParticle, GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}
}

void AAuraBullet::OnDefinitionConfigured(const FAuraProjectileDefinition& Definition)
{
	Super::OnDefinitionConfigured(Definition);

	TracerMesh->SetStaticMesh(Cast<UStaticMesh>(Definition.TracerMesh.TryLoad()));
	TracerMesh->SetRelativeScale3D(Definition.MeshScale);
	FlightParticle = Cast<UParticleSystem>(Definition.FlightParticle.TryLoad());
	BulletImpactParticle = Cast<UParticleSystem>(Definition.ImpactParticle.TryLoad());
	BulletImpactSound = Cast<USoundBase>(Definition.ImpactSound.TryLoad());
	BulletHoleMaterial = Cast<UMaterialInterface>(Definition.SurfaceMarkMaterial.TryLoad());
	BulletHoleSize = Definition.SurfaceMarkSize;
	BulletHoleLifeSpan = Definition.SurfaceMarkLifeSpan;
	// Also handle a definition arriving through RepNotify after BeginPlay.
	if (HasActorBegunPlay()) StartFlightEffects();

	UE_LOG(LogAura, Log, TEXT("[Bullet] Definition=%s tracer=%s flightParticle=%s impactParticle=%s surfaceMark=%s"),
		*Definition.Name.ToString(), *GetNameSafe(TracerMesh->GetStaticMesh()), *GetNameSafe(FlightParticle),
		*GetNameSafe(BulletImpactParticle), *GetNameSafe(BulletHoleMaterial));
}

void AAuraBullet::OnHit()
{
	// Only authoritative impacts can commit bHit on a bullet. Client overlaps or
	// destruction/expiry must not suppress a later server wall mark or invent one.
	if (HasAuthority()) Super::OnHit();
}

void AAuraBullet::OnHitAtSurface(const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	if (HasAuthority()) Super::OnHitAtSurface(ImpactLocation, ImpactNormal);
}

void AAuraBullet::PlayImpactEffects(const FVector& ImpactLocation)
{
	if (FlightParticleComponent)
	{
		FlightParticleComponent->DeactivateSystem();
		FlightParticleComponent->DestroyComponent();
		FlightParticleComponent = nullptr;
	}
	if (GetNetMode() == NM_DedicatedServer) return;
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

void AAuraBullet::PlayImpactEffectsAtSurface(const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	PlayImpactEffects(ImpactLocation);
	if (GetNetMode() == NM_DedicatedServer) return;

	if (!BulletHoleMaterial)
	{
		UE_LOG(LogAura, Warning, TEXT("[Bullet] Surface mark skipped: no material configured for %s"), *GetNameSafe(this));
		return;
	}

	const FVector SurfaceNormal = ImpactNormal.GetSafeNormal(SMALL_NUMBER, -GetActorForwardVector());
	const FRotator DecalRotation = FRotationMatrix::MakeFromX(-SurfaceNormal).Rotator();
	UDecalComponent* BulletHole = UGameplayStatics::SpawnDecalAtLocation(
		this, BulletHoleMaterial, FVector(2.f, BulletHoleSize, BulletHoleSize),
		ImpactLocation + SurfaceNormal * 0.5f, DecalRotation, BulletHoleLifeSpan);

	UE_LOG(LogAura, Log, TEXT("[Bullet] Surface mark %s at=%s normal=%s size=%.1f life=%.1f"),
		BulletHole ? TEXT("spawned") : TEXT("failed"), *ImpactLocation.ToCompactString(),
		*SurfaceNormal.ToCompactString(), BulletHoleSize, BulletHoleLifeSpan);
}
