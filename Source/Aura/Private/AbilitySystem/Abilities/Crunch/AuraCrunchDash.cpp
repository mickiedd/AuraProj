// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchDash.h"

#include "AbilitySystem/Abilities/Crunch/AuraCrunchTagUtils.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
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

	if (DashMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, DashMontage);
		if (MontageTask)
		{
			MontageTask->OnBlendOut.AddDynamic(this, &UAuraCrunchDash::K2_EndAbility);
			MontageTask->OnCompleted.AddDynamic(this, &UAuraCrunchDash::K2_EndAbility);
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchDash::K2_EndAbility);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchDash::K2_EndAbility);
			MontageTask->ReadyForActivation();
		}
	}

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
		World->GetTimerManager().SetTimer(DashTimeoutTimer, this, &UAuraCrunchDash::HandleDashTimeout, 0.75f, false);
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
	if (bDashActive || !IsCrunchActivationActive()) return;
	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character) return;
	DashDirection = Character->GetActorForwardVector().GetSafeNormal2D();
	if (DashDirection.IsNearlyZero()) return;
	DashElapsed = 0.f;
	bDashActive = true;
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		PreviousMaxWalkSpeed = Movement->MaxWalkSpeed;
		PreviousBrakingDeceleration = Movement->BrakingDecelerationWalking;
		bMovementTuningSaved = true;
		Movement->MaxWalkSpeed = DashSpeed;
		Movement->BrakingDecelerationWalking = 100000.f;
	}
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
	Character->AddMovementInput(DashDirection, 1.f, false);
	DashElapsed += 0.02f;
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
	if (DashElapsed >= DashDuration)
	{
		StopDash();
		K2_EndAbility();
	}
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
	if (!bDashActive && !bMovementTuningSaved) return;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTickTimer);
	}
	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			if (bMovementTuningSaved)
			{
				Movement->MaxWalkSpeed = PreviousMaxWalkSpeed;
				Movement->BrakingDecelerationWalking = PreviousBrakingDeceleration;
			}
			Movement->StopMovementImmediately();
		}
	}
	bMovementTuningSaved = false;
	bDashActive = false;
}
