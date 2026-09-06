// Copyright Druid Mechanics
#include "AbilitySystem/Abilities/Crunch/AuraCrunchTornado.h"
#include "AbilitySystem/Abilities/Crunch/AuraCrunchTagUtils.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "AuraGameplayTags.h"
#include "Aura/AuraLogChannels.h"
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
	// Tornado owns a bounded direct-hit cadence, not the base ability's random DoT.
	DebuffChance = 0.f;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_Tornado_Aura.AM_Tornado_Aura"));
	if (MontageFinder.Succeeded()) TornadoMontage = MontageFinder.Object;
}

void UAuraCrunchTornado::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!GetWorld() || !FMath::IsFinite(TornadoDuration) || TornadoDuration <= 0.f
		|| !FMath::IsFinite(HitInterval) || HitInterval < 0.01f
		|| !BeginCrunchActivation(ActorInfo, ActivationInfo) || !K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}
	TornadoEventIndex = 0;
	// Register everything before montage activation: it may synchronously cancel.
	GetWorld()->GetTimerManager().SetTimer(TimeoutTimer, this, &UAuraCrunchTornado::HandleTornadoTimeout, TornadoDuration, false);
	RegisterCrunchTimer(TimeoutTimer);
	if (ActorInfo->IsNetAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(HitTimer, this, &UAuraCrunchTornado::TickTornado, HitInterval, true);
		RegisterCrunchTimer(HitTimer);
		FGameplayCueParameters Cue;
		Cue.RawMagnitude = TornadoRadius;
		// The server owns the persistent cue; the predicting client must receive it too.
		FScopedPredictionWindow CuePrediction(GetAbilitySystemComponentFromActorInfo(), FPredictionKey(), false);
		GetAbilitySystemComponentFromActorInfo()->AddGameplayCue(AuraCrunchTags::Request(TEXT("GameplayCue.Crunch.Tornado")), Cue);
		bTornadoCueActive = true;
	}
	UE_LOG(LogAura, Log, TEXT("[CrunchTornado] Begin authority=%d duration=%.2f radius=%.0f interval=%.2f montage=%s"),
		ActorInfo->IsNetAuthority(), TornadoDuration, TornadoRadius, HitInterval, *GetNameSafe(TornadoMontage));
	if (TornadoMontage && TornadoMontage->GetPlayLength() > 0.f)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TornadoMontage,
			TornadoMontage->GetPlayLength() / TornadoDuration);
		if (MontageTask)
		{
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchTornado::K2_EndAbility);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchTornado::K2_EndAbility);
			MontageTask->ReadyForActivation();
		}
	}
}

void UAuraCrunchTornado::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive()) return;
	CleanupCrunchActivation();
	if (bTornadoCueActive)
	{
		bTornadoCueActive = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FScopedPredictionWindow CuePrediction(ASC, FPredictionKey(), false);
			ASC->RemoveGameplayCue(AuraCrunchTags::Request(TEXT("GameplayCue.Crunch.Tornado")));
		}
	}
	// Let GAS destroy the montage task with AbilityEnded=true so cancellation stops playback.
	// Remove callbacks first to avoid a recursive EndAbility while stopping the montage.
	if (MontageTask)
	{
		MontageTask->OnCancelled.RemoveAll(this);
		MontageTask->OnInterrupted.RemoveAll(this);
	}
	MontageTask = nullptr;
	UE_LOG(LogAura, Log, TEXT("[CrunchTornado] End authority=%d ticks=%d cancelled=%d"),
		ActorInfo && ActorInfo->IsNetAuthority(), TornadoEventIndex, bWasCancelled);
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraCrunchTornado::HandleTornadoTimeout()
{
	if (IsCrunchActivationActive()) K2_EndAbility();
}

void UAuraCrunchTornado::TickTornado()
{
	if (!IsCrunchActivationActive()) return;
	ProcessTornadoHit(TornadoEventIndex++);
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
		FVector PushDirection = (Target->GetActorLocation() - Avatar->GetActorLocation()).GetSafeNormal2D();
		if (PushDirection.IsNearlyZero()) PushDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
		const bool bApplied = ApplyCrunchDamageOnce(Target, FName(*FString::Printf(TEXT("Tornado_%d"), EventIndex)), 0.f,
			PushDirection * HitPushSpeed, FAuraGameplayTags::Get().Damage_Physical);
		UE_LOG(LogAura, Verbose, TEXT("[CrunchTornado] Hit tick=%d target=%s applied=%d"), EventIndex, *GetNameSafe(Target), bApplied);
	}
}
