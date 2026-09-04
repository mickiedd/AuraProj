// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchUppercut.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "Aura/AuraLogChannels.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FGameplayTag CrunchUppercutTag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}
}

UAuraCrunchUppercut::UAuraCrunchUppercut()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(CrunchUppercutTag(TEXT("Abilities.Melee.CrunchUppercut")));
	SetAssetTags(AssetTags);
	BlockAbilitiesWithTag.AddTag(CrunchUppercutTag(TEXT("Abilities.Melee.CrunchCombo")));
	StartupInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.1")), false);
	CrunchStartupInputTagName = FName(TEXT("InputTag.1"));
	CrunchCooldownTag = CrunchUppercutTag(TEXT("Cooldown.Melee.CrunchUppercut"));
	CrunchManaCost = 0.f; // Source GE custom magnitudes are not exposed by public reflection.
	CrunchCooldown = 0.f;
	DefaultCrunchDamage = 35.f;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_UpperCut.AM_UpperCut"));
	if (MontageFinder.Succeeded())
	{
		UpperCutMontage = MontageFinder.Object;
	}
}

void UAuraCrunchUppercut::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!BeginCrunchActivation(ActorInfo, ActivationInfo) || !K2_CommitAbility())
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchUppercut] Activation rejected: actor info, prediction key, or cost invalid."));
		K2_EndAbility();
		return;
	}

	NextFollowupIndex = 0;
	NextDamageEventIndex = 0;
	bLaunchCommitted = false;
	bFollowupWindowOpen = false;

	if (UpperCutMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, UpperCutMontage);
		if (MontageTask)
		{
			MontageTask->OnBlendOut.AddDynamic(this, &UAuraCrunchUppercut::K2_EndAbility);
			MontageTask->OnCompleted.AddDynamic(this, &UAuraCrunchUppercut::K2_EndAbility);
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchUppercut::K2_EndAbility);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchUppercut::K2_EndAbility);
			MontageTask->ReadyForActivation();
		}
	}

	if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		LaunchEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, CrunchUppercutTag(TEXT("Ability.Uppercut.Launch")), nullptr, false, false);
		DamageEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, CrunchUppercutTag(TEXT("Ability.Generic.Damage")), nullptr, false, false);
		if (LaunchEventTask)
		{
			LaunchEventTask->EventReceived.AddDynamic(this, &UAuraCrunchUppercut::HandleLaunchEvent);
			LaunchEventTask->ReadyForActivation();
		}
		if (DamageEventTask)
		{
			DamageEventTask->EventReceived.AddDynamic(this, &UAuraCrunchUppercut::HandleDamageEvent);
			DamageEventTask->ReadyForActivation();
		}
		ArmFollowupInput();
		if (ActorInfo->IsNetAuthority())
		{
			ScheduleSourceTimingFallbacks();
		}
	}
}

void UAuraCrunchUppercut::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (MontageTask) MontageTask->EndTask();
	if (LaunchEventTask) LaunchEventTask->EndTask();
	if (DamageEventTask) DamageEventTask->EndTask();
	if (FollowupInputTask) FollowupInputTask->EndTask();
	MontageTask = nullptr;
	LaunchEventTask = nullptr;
	DamageEventTask = nullptr;
	FollowupInputTask = nullptr;
	bFollowupWindowOpen = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraCrunchUppercut::HandleLaunchEvent(FGameplayEventData Payload)
{
	CommitLaunch();
}

void UAuraCrunchUppercut::HandleDamageEvent(FGameplayEventData Payload)
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || NextDamageEventIndex >= DamageEventTimes.Num()) return;
	CommitDamageEvent(NextDamageEventIndex++, &Payload.TargetData);
}

void UAuraCrunchUppercut::CommitLaunch()
{
	if (bLaunchCommitted || !IsCrunchActivationActive()) return;
	bLaunchCommitted = true;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;
	LaunchCrunchCharacter(Avatar, FVector::UpVector * UpperCutLaunchSpeed);
	TArray<AActor*> Targets;
	CollectCrunchTargets(Avatar->GetActorLocation(), UpperCutTargetRadius, Targets, UpperCutTargetRange);
	for (AActor* Target : Targets)
	{
		LaunchCrunchCharacter(Target, FVector::UpVector * UpperCutLaunchSpeed);
		ApplyCrunchDamageOnce(Target, FName(TEXT("UppercutLaunch")), 0.f, FVector::ZeroVector,
			FAuraGameplayTags::Get().Damage_Physical);
	}
	UE_LOG(LogAura, Log, TEXT("[CrunchUppercut] Launch committed targets=%d speed=%.1f"), Targets.Num(), UpperCutLaunchSpeed);
}

void UAuraCrunchUppercut::CommitDamageEvent(int32 EventIndex, const FGameplayAbilityTargetDataHandle* OptionalTargetData)
{
	if (!IsCrunchActivationActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) return;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;
	const FName EventKey = FName(*FString::Printf(TEXT("UppercutAir_%d"), EventIndex));
	if (OptionalTargetData && OptionalTargetData->Num() > 0)
	{
		const int32 Count = UAbilitySystemBlueprintLibrary::GetDataCountFromTargetData(*OptionalTargetData);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FHitResult Hit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(*OptionalTargetData, Index);
			if (AActor* Target = Hit.GetActor())
			{
				ApplyCrunchDamageOnce(Target, EventKey, 0.f, FVector::UpVector * UpperCutHoldSpeed,
					FAuraGameplayTags::Get().Damage_Physical);
			}
		}
	}
	else
	{
		TArray<AActor*> Targets;
		CollectCrunchTargets(Avatar->GetActorLocation(), UpperCutTargetRadius, Targets, UpperCutTargetRange);
		for (AActor* Target : Targets)
		{
			ApplyCrunchDamageOnce(Target, EventKey, 0.f, FVector::UpVector * UpperCutHoldSpeed,
				FAuraGameplayTags::Get().Damage_Physical);
		}
	}
}

void UAuraCrunchUppercut::HandleFollowupInput(float TimeWaited)
{
	if (bFollowupWindowOpen && UpperCutMontage && NextFollowupIndex < FollowupSections.Num())
	{
		if (UAnimInstance* AnimInstance = GetOwningComponentFromActorInfo()
			? GetOwningComponentFromActorInfo()->GetAnimInstance() : nullptr)
		{
			const FName CurrentSection = AnimInstance->Montage_GetCurrentSection(UpperCutMontage);
			AnimInstance->Montage_SetNextSection(CurrentSection, FollowupSections[NextFollowupIndex], UpperCutMontage);
			++NextFollowupIndex;
		}
	}
	ArmFollowupInput();
}

void UAuraCrunchUppercut::ArmFollowupInput()
{
	if (!IsCrunchActivationActive()) return;
	if (FollowupInputTask) FollowupInputTask->EndTask();
	FollowupInputTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (FollowupInputTask)
	{
		FollowupInputTask->OnPress.AddDynamic(this, &UAuraCrunchUppercut::HandleFollowupInput);
		FollowupInputTask->ReadyForActivation();
	}
}

void UAuraCrunchUppercut::ScheduleSourceTimingFallbacks()
{
	UWorld* World = GetWorld();
	if (!World) return;
	FTimerHandle LaunchTimer;
	World->GetTimerManager().SetTimer(LaunchTimer, this, &UAuraCrunchUppercut::CommitLaunch, LaunchEventTime, false);
	RegisterCrunchTimer(LaunchTimer);
	for (int32 Index = 0; Index < DamageEventTimes.Num(); ++Index)
	{
		FTimerHandle DamageTimer;
		FTimerDelegate Delegate;
		Delegate.BindWeakLambda(this, [this, Index]() { CommitDamageEvent(Index); });
		World->GetTimerManager().SetTimer(DamageTimer, Delegate, DamageEventTimes[Index], false);
		RegisterCrunchTimer(DamageTimer);
	}
	FTimerHandle OpenTimer;
	World->GetTimerManager().SetTimer(OpenTimer, this, &UAuraCrunchUppercut::OpenFollowupWindow, 0.55f, false);
	RegisterCrunchTimer(OpenTimer);
	FTimerHandle CloseTimer;
	World->GetTimerManager().SetTimer(CloseTimer, this, &UAuraCrunchUppercut::CloseFollowupWindow, 2.05f, false);
	RegisterCrunchTimer(CloseTimer);
	World->GetTimerManager().SetTimer(UpperCutTimeoutTimer, this, &UAuraCrunchUppercut::HandleUpperCutTimeout, 3.2f, false);
	RegisterCrunchTimer(UpperCutTimeoutTimer);
}

void UAuraCrunchUppercut::OpenFollowupWindow()
{
	bFollowupWindowOpen = true;
}

void UAuraCrunchUppercut::CloseFollowupWindow()
{
	bFollowupWindowOpen = false;
}

void UAuraCrunchUppercut::HandleUpperCutTimeout()
{
	if (IsCrunchActivationActive())
	{
		K2_EndAbility();
	}
}
