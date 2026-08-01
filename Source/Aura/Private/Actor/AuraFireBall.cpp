// Copyright Druid Mechanics


#include "Actor/AuraFireBall.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "GameplayCueManager.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Aura/AuraLogChannels.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

AAuraFireBall::AAuraFireBall()
{
	// The FireBall travels out and returns to its caster; it must pass through walls rather
	// than stop on them, so opt out of the base class's Block-on-WorldStatic behavior.
	if (Sphere)
	{
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	}
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
	TryStartOutgoingTimeline();
}

void AAuraFireBall::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsValidOverlap(OtherActor)) return;

	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
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
	TryStartOutgoingTimeline();
}

void AAuraFireBall::TryStartOutgoingTimeline()
{
	if (bOutgoingTimelineStarted)
	{
		return;
	}

	if (!IsValid(ReturnToActor))
	{
		UE_LOG(LogAura, Verbose, TEXT("[FireBall] Timeline waiting for ReturnToActor: Actor=%s"), *GetNameSafe(this));
		return;
	}

	bOutgoingTimelineStarted = true;
	UE_LOG(LogAura, Log, TEXT("[FireBall] Starting outgoing timeline: Actor=%s ReturnToActor=%s"),
		*GetNameSafe(this), *GetNameSafe(ReturnToActor));
	StartOutgoingTimeline();
}
