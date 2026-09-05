// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchTornado.h"

#include "AbilitySystem/Abilities/Crunch/AuraCrunchTagUtils.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AuraGameplayTags.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UAuraCrunchTornado::UAuraCrunchTornado()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AuraCrunchTags::Request(TEXT("Abilities.Melee.CrunchTornado")));
	SetAssetTags(AssetTags);
	StartupInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.4")), false);
	CrunchStartupInputTagName = FName(TEXT("InputTag.4"));
	CrunchCooldownTag = AuraCrunchTags::Request(TEXT("Cooldown.Melee.CrunchTornado"));
	CrunchManaCost = 0.f;
	CrunchCooldown = 0.f;
	DefaultCrunchDamage = 20.f;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_Tornado.AM_Tornado"));
	if (MontageFinder.Succeeded()) TornadoMontage = MontageFinder.Object;
}

void UAuraCrunchTornado::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!BeginCrunchActivation(ActorInfo, ActivationInfo) || !K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}
	TornadoElapsed = 0.f;
	TornadoEventIndex = 0;
	if (TornadoMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TornadoMontage);
		if (MontageTask)
		{
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchTornado::K2_EndAbility);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchTornado::K2_EndAbility);
			MontageTask->ReadyForActivation();
		}
	}
	DamageEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, AuraCrunchTags::Request(TEXT("Ability.Generic.Damage")), nullptr, false, false);
	if (DamageEventTask)
	{
		DamageEventTask->EventReceived.AddDynamic(this, &UAuraCrunchTornado::HandleAuthoredDamageEvent);
		DamageEventTask->ReadyForActivation();
	}
	if (UWorld* World = GetWorld())
	{
		// A single bounded repeating timer is used instead of an unbounded next-tick loop.
		World->GetTimerManager().SetTimer(HitTimer, this, &UAuraCrunchTornado::TickTornado, HitInterval, true);
		World->GetTimerManager().SetTimer(TimeoutTimer, this, &UAuraCrunchTornado::HandleTornadoTimeout, TornadoDuration, false);
		RegisterCrunchTimer(HitTimer);
		RegisterCrunchTimer(TimeoutTimer);
	}
}

void UAuraCrunchTornado::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask) MontageTask->EndTask();
	if (DamageEventTask) DamageEventTask->EndTask();
	MontageTask = nullptr;
	DamageEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraCrunchTornado::HandleAuthoredDamageEvent(FGameplayEventData Payload)
{
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		const int32 EventIndex = FMath::Max(0, FMath::RoundToInt(TornadoElapsed / FMath::Max(HitInterval, 0.01f)));
		ProcessTornadoHit(EventIndex);
	}
}

void UAuraCrunchTornado::HandleTornadoTimeout()
{
	if (IsCrunchActivationActive()) K2_EndAbility();
}

void UAuraCrunchTornado::TickTornado()
{
	if (!IsCrunchActivationActive()) return;
	ProcessTornadoHit(TornadoEventIndex++);
	TornadoElapsed += HitInterval;
}

void UAuraCrunchTornado::ProcessTornadoHit(int32 EventIndex)
{
	if (!IsCrunchActivationActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;
	TArray<AActor*> Targets;
	CollectCrunchTargets(Avatar->GetActorLocation(), TornadoRadius, Targets, TornadoRadius);
	for (AActor* Target : Targets)
	{
		const FVector PushDirection = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal();
		ApplyCrunchDamageOnce(Target, FName(*FString::Printf(TEXT("Tornado_%d"), EventIndex)), 0.f,
			PushDirection * HitPushSpeed, FAuraGameplayTags::Get().Damage_Physical);
	}
}
