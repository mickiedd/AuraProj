// Copyright Druid Mechanics


#include "Actor/AuraProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Aura/Aura.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Aura/AuraLogChannels.h"
#include "Data/AuraGameplayConfig.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "WorldCollision.h"

AAuraProjectile::AAuraProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetReplicateMovement(true);

	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(15.f);
	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	// Block (not Overlap) WorldStatic so the projectile physically stops against walls and
	// fires OnComponentHit; an Overlap-only QueryOnly sphere would pass straight through.
	// AAuraFireBall opts back out to Ignore (it returns to its caster and must fly through geometry).
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>("ProjectileMesh");
	ProjectileMesh->SetupAttachment(Sphere);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetRelativeScale3D(FVector(0.3f));

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void AAuraProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAuraProjectile, ProjectileDefinitionName);
}

bool AAuraProjectile::ConfigureFromDefinition(FName InDefinitionName)
{
	const FAuraProjectileDefinition* Definition = FAuraGameplayConfig::FindProjectile(InDefinitionName);
	if (!Definition)
	{
		UE_LOG(LogAura, Error, TEXT("[Projectile] Unknown projectile definition '%s'"), *InDefinitionName.ToString());
		return false;
	}
	if (!GetClass()->IsChildOf(Definition->NativeClass))
	{
		UE_LOG(LogAura, Error, TEXT("[Projectile] Definition '%s' requires %s, got %s"), *InDefinitionName.ToString(), *GetNameSafe(Definition->NativeClass), *GetNameSafe(GetClass()));
		return false;
	}

	ProjectileDefinitionName = InDefinitionName;
	LifeSpan = Definition->LifeSpan;
	Sphere->SetSphereRadius(Definition->CollisionRadius);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, Definition->WorldStaticResponse);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ProjectileMovement->InitialSpeed = Definition->InitialSpeed;
	ProjectileMovement->MaxSpeed = Definition->MaxSpeed;
	ProjectileMovement->ProjectileGravityScale = Definition->GravityScale;
	ProjectileMesh->SetStaticMesh(Cast<UStaticMesh>(Definition->Mesh.IsNull() ? nullptr : Definition->Mesh.TryLoad()));
	ProjectileMesh->SetRelativeScale3D(Definition->MeshScale);
	FlightTrail = Cast<UNiagaraSystem>(Definition->FlightTrail.IsNull() ? nullptr : Definition->FlightTrail.TryLoad());
	ImpactEffect = Cast<UNiagaraSystem>(Definition->ImpactEffect.IsNull() ? nullptr : Definition->ImpactEffect.TryLoad());
	ImpactSound = Cast<USoundBase>(Definition->ImpactSound.IsNull() ? nullptr : Definition->ImpactSound.TryLoad());
	LoopingSound = Cast<USoundBase>(Definition->LoopingSound.IsNull() ? nullptr : Definition->LoopingSound.TryLoad());
	OnDefinitionConfigured(*Definition);
	return true;
}

void AAuraProjectile::OnRep_ProjectileDefinition()
{
	ConfigureFromDefinition(ProjectileDefinitionName);
}

void AAuraProjectile::OnDefinitionConfigured(const FAuraProjectileDefinition& Definition)
{
}

void AAuraProjectile::BeginPlay()
{
	Super::BeginPlay();
	if (!ProjectileDefinitionName.IsNone()) ConfigureFromDefinition(ProjectileDefinitionName);
	UE_LOG(LogAura, Log, TEXT("[Projectile] BeginPlay: Actor=%s Role=%d RemoteRole=%d Loc=%s"),
		*GetNameSafe(this), (int32)GetLocalRole(), (int32)GetRemoteRole(), *GetActorLocation().ToCompactString());

	// Log the *actual* runtime collision responses the Sphere ended up with. BP subclasses
	// (e.g. BP_FireBolt) bake the Sphere's responses into the asset at compile time, which
	// overrides the C++ constructor defaults — this log surfaces that (pre-reassert) so a
	// projectile that tunnels through walls can be diagnosed.
	UE_LOG(LogAura, Log, TEXT("[Projectile] BeginPlay: pre-reassert Sphere WorldStatic=%d WorldDynamic=%d Pawn=%d Enabled=%d"),
		(int32)Sphere->GetCollisionResponseToChannel(ECC_WorldStatic),
		(int32)Sphere->GetCollisionResponseToChannel(ECC_WorldDynamic),
		(int32)Sphere->GetCollisionResponseToChannel(ECC_Pawn),
		(int32)Sphere->GetCollisionEnabled());

	SetLifeSpan(LifeSpan);
	SetReplicateMovement(true);

	// Re-assert collision responses at runtime so BP-baked values can't override the fix.
	// WorldStatic MUST be Block or the projectile's sweep is non-blocking and it tunnels
	// through walls (the bug). AAuraFireBall::BeginPlay re-opts-out to Ignore afterward
	// (the FireBall returns to its caster and must fly through geometry).
	// Also force collision ENABLED: BP_FireBolt bakes Sphere collision as NoCollision (a
	// 0.1s-delayed "Set Collision Enabled" node is meant to re-enable it but never takes effect,
	// so the projectile never collides). Self-collision is guarded (SourceAvatarActor check), so
	// enabling from frame 0 is safe.
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	if (ProjectileDefinitionName.IsNone()) Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AAuraProjectile::OnSphereOverlap);
	Sphere->OnComponentHit.AddDynamic(this, &AAuraProjectile::OnSphereHit);
	UE_LOG(LogAura, Log, TEXT("[Projectile] BeginPlay: post-reassert WorldStatic=%d Enabled=%d overlapBound=%d hitBound=%d"),
		(int32)Sphere->GetCollisionResponseToChannel(ECC_WorldStatic),
		(int32)Sphere->GetCollisionEnabled(),
		(int32)Sphere->OnComponentBeginOverlap.IsBound(),
		(int32)Sphere->OnComponentHit.IsBound());

	// DIAGNOSTIC: forward probe along the firing direction. Tells us whether a wall (WorldStatic)
	// or a pawn is actually in the bolt's path at spawn, and how far. If a wall is ahead at dist X
	// but the bolt later expires at a location past X with bHit=0, the movement sweep is passing
	// through the wall (not a homing-into-open-space issue). Probe with ECC_Projectile (the bolt's
	// own channel) so it reflects what the bolt's collision would see.
	{
		FHitResult ProbeHit;
		FCollisionQueryParams QP(SCENE_QUERY_STAT(ProjProbe), false, this);
		if (GetOwner()) QP.AddIgnoredActor(GetOwner());
		FCollisionShape Sph = FCollisionShape::MakeSphere(16.f);
		const FVector Start = GetActorLocation();
		const FVector End = Start + GetActorForwardVector() * 5000.f;
		const bool bBlocked = GetWorld()->SweepSingleByChannel(ProbeHit, Start, End, FQuat::Identity, ECC_Projectile, Sph, QP);
		UE_LOG(LogAura, Log, TEXT("[Projectile] BeginPlay: fwdProbe(ECC_Projectile) blocked=%d actor=%s comp=%s dist=%.0f"),
			(int32)bBlocked, *GetNameSafe(ProbeHit.GetActor()), *GetNameSafe(ProbeHit.GetComponent()), ProbeHit.Distance);
	}

	if (FlightTrail && !FlightTrailComponent)
	{
		FlightTrailComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(FlightTrail, GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true, true);
	}

	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());
}

void AAuraProjectile::OnHit()
{
	UE_LOG(LogAura, Log, TEXT("[Projectile] OnHit: Actor=%s bHit=%d HasAuth=%d Loc=%s"),
		*GetNameSafe(this), (int32)bHit, (int32)HasAuthority(), *GetActorLocation().ToCompactString());
	if (bHit)
	{
		return;
	}

	if (HasAuthority())
	{
		MulticastPlayImpactEffects(GetActorLocation());
		return;
	}

	PlayImpactEffects(GetActorLocation());
	bHit = true;
}

void AAuraProjectile::MulticastPlayImpactEffects_Implementation(const FVector_NetQuantize& ImpactLocation)
{
	if (bHit)
	{
		return;
	}

	PlayImpactEffects(ImpactLocation);
	bHit = true;
}

void AAuraProjectile::PlayImpactEffects(const FVector& ImpactLocation)
{
	UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactLocation, FRotator::ZeroRotator);
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, ImpactLocation);
	StopLoopingSound();
}

void AAuraProjectile::StopLoopingSound()
{
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
		LoopingSoundComponent = nullptr;
	}
}

void AAuraProjectile::Destroyed()
{
	StopLoopingSound();
	UE_LOG(LogAura, Log, TEXT("[Projectile] Destroyed: Actor=%s bHit=%d HasAuth=%d Loc=%s"),
		*GetNameSafe(this), (int32)bHit, (int32)HasAuthority(), *GetActorLocation().ToCompactString());
	if (!bHit && !HasAuthority()) OnHit();
	Super::Destroyed();
}

void AAuraProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AActor* SourceAvatarActor = UAuraAbilitySystemLibrary::GetSafeAvatarActor(DamageEffectParams.SourceAbilitySystemComponent);
	if (SourceAvatarActor == OtherActor) return;
	if (bHit) return;

	UE_LOG(LogAura, Log, TEXT("[Projectile] Overlap: Actor=%s Other=%s Loc=%s bHasASC=%s"),
		*GetNameSafe(this), *GetNameSafe(OtherActor), *GetActorLocation().ToCompactString(),
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor) ? TEXT("true") : TEXT("false"));

	ApplyImpactAndDestroy(OtherActor);
}

void AAuraProjectile::OnSphereHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	AActor* SourceAvatarActor = UAuraAbilitySystemLibrary::GetSafeAvatarActor(DamageEffectParams.SourceAbilitySystemComponent);
	if (SourceAvatarActor == OtherActor) return;
	if (bHit) return;

	UE_LOG(LogAura, Log, TEXT("[Projectile] Hit(block): Actor=%s Other=%s Loc=%s bHasASC=%s"),
		*GetNameSafe(this), *GetNameSafe(OtherActor), *GetActorLocation().ToCompactString(),
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor) ? TEXT("true") : TEXT("false"));

	ApplyImpactAndDestroy(OtherActor);
}

void AAuraProjectile::ApplyImpactAndDestroy(AActor* OtherActor)
{
	OnHit();

	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
			DamageEffectParams.DeathImpulse = DeathImpulse;
			const bool bKnockback = FMath::RandRange(1, 100) < DamageEffectParams.KnockbackChance;
			if (bKnockback)
			{
				FRotator Rotation = GetActorRotation();
				Rotation.Pitch = 45.f;

				const FVector KnockbackDirection = Rotation.Vector();
				const FVector KnockbackForce = KnockbackDirection * DamageEffectParams.KnockbackForceMagnitude;
				DamageEffectParams.KnockbackForce = KnockbackForce;
			}

			DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
			UAuraAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
		}

		Destroy();
	}
}

bool AAuraProjectile::IsValidOverlap(AActor* OtherActor)
{
	if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return false;
	AActor* SourceAvatarActor = UAuraAbilitySystemLibrary::GetSafeAvatarActor(DamageEffectParams.SourceAbilitySystemComponent);
	if (SourceAvatarActor == OtherActor) return false;

	return true;
}
