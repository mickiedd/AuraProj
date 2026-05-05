// Copyright Druid Mechanics


#include "Actor/AuraFireBall.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "GameplayCueManager.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Aura/AuraLogChannels.h"
#include "Components/AudioComponent.h"
#include "Net/UnrealNetwork.h"

void AAuraFireBall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAuraFireBall, ReturnToActor);
}

void AAuraFireBall::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogAura, Log, TEXT("[FireBall] BeginPlay: Actor=%s Role=%d RemoteRole=%d ReturnToActor=%s"),
		*GetNameSafe(this), (int32)GetLocalRole(), (int32)GetRemoteRole(), *GetNameSafe(ReturnToActor));
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

	if (GetOwner())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location = GetActorLocation();
		UGameplayCueManager::ExecuteGameplayCue_NonReplicated(GetOwner(), FAuraGameplayTags::Get().GameplayCue_FireBlast, CueParams);
	}
	
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	bHit = true;
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
