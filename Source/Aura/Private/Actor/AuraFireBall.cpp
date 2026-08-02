// Copyright Druid Mechanics


#include "Actor/AuraFireBall.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "GameplayCueManager.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Aura/AuraLogChannels.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Data/AuraGameplayConfig.h"
#include "Net/UnrealNetwork.h"

AAuraFireBall::AAuraFireBall()
{
	PrimaryActorTick.bCanEverTick = true;
	// The FireBall travels out and returns to its caster; it must pass through walls rather
	// than stop on them, so opt out of the base class's Block-on-WorldStatic behavior.
	if (Sphere)
	{
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	}
}

void AAuraFireBall::OnDefinitionConfigured(const FAuraProjectileDefinition& Definition)
{
	Super::OnDefinitionConfigured(Definition);
	OutboundDistance = Definition.OutboundDistance;
	OutboundDuration = Definition.OutboundDuration;
	ReturnSpeed = Definition.ReturnSpeed;
	ReturnDistance = Definition.ReturnDistance;
}

void AAuraFireBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAuraFireBall, ReturnToActor);
}

void AAuraFireBall::BeginPlay()
{
	Super::BeginPlay();
	// Super::BeginPlay re-asserts WorldStatic=Block (so BP-baked values can't break the base
	// projectile). The FireBall must pass through walls (it returns to its caster), so opt back
	// out to Ignore *after* Super::BeginPlay.
	if (Sphere)
	{
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	}
	UE_LOG(LogAura, Log, TEXT("[FireBall] BeginPlay: Actor=%s Role=%d RemoteRole=%d ReturnToActor=%s WorldStatic=%d"),
		*GetNameSafe(this), (int32)GetLocalRole(), (int32)GetRemoteRole(), *GetNameSafe(ReturnToActor),
		(int32)Sphere->GetCollisionResponseToChannel(ECC_WorldStatic));
	TryStartNativeMovement();
}

void AAuraFireBall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || MovementState == EFireBallMovementState::Waiting || MovementState == EFireBallMovementState::Finished) return;
	if (!IsValid(ReturnToActor))
	{
		MovementState = EFireBallMovementState::Finished;
		Destroy();
		return;
	}

	if (MovementState == EFireBallMovementState::Outgoing)
	{
		MovementElapsed += DeltaSeconds;
		const float Alpha = OutboundDuration > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(MovementElapsed / OutboundDuration, 0.f, 1.f) : 1.f;
		SetActorLocation(FMath::Lerp(OutgoingStart, OutgoingEnd, FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f)), true);
		if (Alpha >= 1.f) MovementState = EFireBallMovementState::Returning;
		return;
	}

	const FVector Destination = ReturnToActor->GetActorLocation();
	const FVector Current = GetActorLocation();
	if (FVector::DistSquared(Current, Destination) <= FMath::Square(ReturnDistance))
	{
		FinishReturn();
		return;
	}
	SetActorLocation(FMath::VInterpConstantTo(Current, Destination, DeltaSeconds, ReturnSpeed), true);
}

void AAuraFireBall::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsValidOverlap(OtherActor)) return;
	if (DamagedActors.Contains(OtherActor)) return;

	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent ? DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
			if (!SourceActor || !UAuraAbilitySystemLibrary::IsNotFriend(SourceActor, OtherActor)) return;
			DamagedActors.Add(OtherActor);
			const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
			DamageEffectParams.DeathImpulse = DeathImpulse;
			
			DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
			UAuraAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
		}
	}
}

void AAuraFireBall::OnHit()
{
	UE_LOG(LogAura, Log, TEXT("[FireBall] OnHit: Actor=%s Location=%s Owner=%s"),
		*GetNameSafe(this), *GetActorLocation().ToCompactString(), *GetNameSafe(GetOwner()));
	Super::OnHit();
}

void AAuraFireBall::PlayImpactEffects(const FVector& ImpactLocation)
{
	if (GetOwner())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = ImpactLocation;
		UGameplayCueManager::ExecuteGameplayCue_NonReplicated(GetOwner(), FAuraGameplayTags::Get().GameplayCue_FireBlast, CueParams);
	}

	StopLoopingSound();
}

void AAuraFireBall::OnRep_ReturnToActor()
{
	UE_LOG(LogAura, Log, TEXT("[FireBall] OnRep_ReturnToActor: Actor=%s ReturnToActor=%s"),
		*GetNameSafe(this), *GetNameSafe(ReturnToActor));
	TryStartNativeMovement();
}

void AAuraFireBall::TryStartNativeMovement()
{
	if (MovementState != EFireBallMovementState::Waiting)
	{
		return;
	}

	if (!IsValid(ReturnToActor))
	{
		UE_LOG(LogAura, Verbose, TEXT("[FireBall] Timeline waiting for ReturnToActor: Actor=%s"), *GetNameSafe(this));
		return;
	}

	OutgoingStart = GetActorLocation();
	OutgoingEnd = OutgoingStart + GetActorForwardVector() * OutboundDistance;
	MovementElapsed = 0.f;
	MovementState = EFireBallMovementState::Outgoing;
}

void AAuraFireBall::FinishReturn()
{
	if (MovementState == EFireBallMovementState::Finished) return;
	MovementState = EFireBallMovementState::Finished;
	OnHit();
	Destroy();
}
