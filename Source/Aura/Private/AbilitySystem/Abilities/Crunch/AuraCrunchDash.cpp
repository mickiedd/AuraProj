// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchDash.h"

#include "AbilitySystem/Abilities/Crunch/AuraCrunchTagUtils.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AuraGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

UAuraCrunchDash::UAuraCrunchDash()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AuraCrunchTags::Request(TEXT("Abilities.Melee.CrunchDash")));
	SetAssetTags(AssetTags);
	StartupInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.2")), false);
	CrunchStartupInputTagName = FName(TEXT("InputTag.2"));
	CrunchCooldownTag = AuraCrunchTags::Request(TEXT("Cooldown.Melee.CrunchDash"));
	CrunchManaCost = 0.f;
	CrunchCooldown = 0.f;
	DefaultCrunchDamage = 30.f;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> MontageFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_Dash.AM_Dash"));
	if (MontageFinder.Succeeded()) DashMontage = MontageFinder.Object;
}

void UAuraCrunchDash::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!BeginCrunchActivation(ActorInfo, ActivationInfo) || !K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}

	if (DashMontage && ActorInfo->GetAnimInstance())
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DashMontage);
		if (MontageTask)
		{
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchDash::K2_EndAbility);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchDash::K2_EndAbility);
			MontageTask->ReadyForActivation();
		}
	}

	if (!IsCrunchActivationActive()) return;

	DashStartEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, AuraCrunchTags::Request(TEXT("Ability.Dash.Start")), nullptr, false, false);
	if (DashStartEventTask)
	{
		DashStartEventTask->EventReceived.AddDynamic(this, &UAuraCrunchDash::HandleDashStartEvent);
		DashStartEventTask->ReadyForActivation();
	}

	// The source notify is retained as an event path when the imported montage can emit it;
	// the bounded fallback keeps server gameplay deterministic when the source notify class
	// is unavailable in a cooked Aura build.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DashStartTimer, this, &UAuraCrunchDash::StartDash, 0.248f, false);
		RegisterCrunchTimer(DashStartTimer);
		World->GetTimerManager().SetTimer(DashTimeoutTimer, this, &UAuraCrunchDash::HandleDashTimeout, 0.248f + FMath::Max(DashDuration, 0.01f) + 0.5f, false);
		RegisterCrunchTimer(DashTimeoutTimer);
	}
}

void UAuraCrunchDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	StopDash();
	if (MontageTask) MontageTask->EndTask();
	if (DashStartEventTask) DashStartEventTask->EndTask();
	MontageTask = nullptr;
	DashStartEventTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraCrunchDash::HandleDashStartEvent(FGameplayEventData Payload)
{
	StartDash();
}

void UAuraCrunchDash::StartDash()
{
	if (bDashStarted || !IsCrunchActivationActive()) return;
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;
	DashDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	if (DashDirection.IsNearlyZero()) return;
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement || !FMath::IsFinite(DashDistance) || DashDistance <= 0.f
		|| !FMath::IsFinite(DashDuration) || DashDuration <= 0.f)
	{
		K2_EndAbility();
		return;
	}
	bDashStarted = true;
	bDashActive = true;
	DashStartLocation = Character->GetActorLocation();
	// Animation remains presentation-only; extracted montage motion must not override the dash source.
	DashAnimInstance = Character->GetMesh()->GetAnimInstance();
	if (DashAnimInstance.IsValid())
	{
		PreviousRootMotionMode = DashAnimInstance->RootMotionMode;
		DashAnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
	DashMovementTask = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, TEXT("CrunchDashMovement"), DashDirection, DashDistance / DashDuration, DashDuration,
		false, nullptr, ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, 0.f, true);
	if (!DashMovementTask)
	{
		K2_EndAbility();
		return;
	}
	DashMovementTask->OnFinish.AddDynamic(this, &UAuraCrunchDash::HandleDashFinished);
	DashMovementTask->ReadyForActivation();
	UE_LOG(LogTemp, Log, TEXT("[CrunchDash] Start Authority=%d Distance=%.1f Duration=%.3f Speed=%.1f"),
		Character->HasAuthority(), DashDistance, DashDuration, DashDistance / DashDuration);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DashTickTimer, this, &UAuraCrunchDash::TickDash, 0.02f, true);
		RegisterCrunchTimer(DashTickTimer);
	}
}

void UAuraCrunchDash::TickDash()
{
	if (!bDashActive || !IsCrunchActivationActive()) return;
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;
	if (Character->HasAuthority())
	{
		TArray<AActor*> Targets;
		CollectCrunchTargets(Character->GetActorLocation(), DashTargetRadius, Targets, DashTargetRadius + 100.f);
		for (AActor* Target : Targets)
		{
			ApplyCrunchDamageOnce(Target, FName(TEXT("Dash")), 0.f,
				DashDirection * DashPushSpeed, FAuraGameplayTags::Get().Damage_Physical);
		}
	}
}

void UAuraCrunchDash::HandleDashFinished()
{
	if (IsCrunchActivationActive()) K2_EndAbility();
}

void UAuraCrunchDash::HandleDashTimeout()
{
	if (IsCrunchActivationActive())
	{
		K2_EndAbility();
	}
}

void UAuraCrunchDash::StopDash()
{
	if (!bDashActive) return;
	bDashActive = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(DashTickTimer);
	if (DashMovementTask) DashMovementTask->EndTask();
	DashMovementTask = nullptr;
	if (DashAnimInstance.IsValid())
	{
		DashAnimInstance->SetRootMotionMode(static_cast<ERootMotionMode::Type>(PreviousRootMotionMode));
	}
	DashAnimInstance.Reset();
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		UE_LOG(LogTemp, Log, TEXT("[CrunchDash] Stop Authority=%d Travelled=%.1f Requested=%.1f"),
			Character->HasAuthority(), FVector::Dist2D(DashStartLocation, Character->GetActorLocation()), DashDistance);
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			// Stop horizontal dash momentum while preserving gravity/falling velocity.
			Movement->Velocity.X = 0.f;
			Movement->Velocity.Y = 0.f;
		}
	}
}
