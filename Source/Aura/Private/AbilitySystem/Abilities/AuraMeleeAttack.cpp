// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/AuraMeleeAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "Aura/AuraLogChannels.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	UAnimMontage* GCrunchTestComboMontageOverride = nullptr;
}
#endif

UAuraMeleeAttack::UAuraMeleeAttack()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> ComboMontageFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_CrunchComboV4.AM_CrunchComboV4"));
	if (ComboMontageFinder.Succeeded())
	{
		ComboMontage = ComboMontageFinder.Object;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (GCrunchTestComboMontageOverride)
	{
		ComboMontage = GCrunchTestComboMontageOverride;
	}
#endif
	FGameplayTagContainer CrunchAbilityTags;
	const FGameplayTag CrunchAbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Melee.CrunchCombo"), false);
	if (CrunchAbilityTag.IsValid())
	{
		CrunchAbilityTags.AddTag(CrunchAbilityTag);
	}
	SetAssetTags(CrunchAbilityTags);
	StartupInputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.LMB"), false);

	DamageEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Crunch.Combo.Damage"), false);
	ComboWindowOpenTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Crunch.Combo.Window.Open"), false);
	ComboWindowCloseTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Crunch.Combo.Window.Close"), false);
}

#if WITH_DEV_AUTOMATION_TESTS
void UAuraMeleeAttack::SetTestComboMontageOverride(UAnimMontage* InMontage)
{
	GCrunchTestComboMontageOverride = InMontage;
}
#endif

void UAuraMeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchCombo] Activation rejected: actor info or ASC is unavailable."));
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchCombo] Activation rejected: authority or prediction permission is unavailable."));
		K2_EndAbility();
		return;
	}
	bool bValidSections = ComboMontage != nullptr && ComboSections.Num() > 0;
	if (bValidSections)
	{
		TSet<FName> UniqueSections;
		for (const FName SectionName : ComboSections)
		{
			if (SectionName.IsNone() || ComboMontage->GetSectionIndex(SectionName) == INDEX_NONE || UniqueSections.Contains(SectionName))
			{
				bValidSections = false;
				break;
			}
			UniqueSections.Add(SectionName);
		}
	}

	if (!bValidSections || !K2_CommitAbility())
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchCombo] Activation rejected: montage, sections, or cost is unavailable."));
		K2_EndAbility();
		return;
	}

	CurrentComboIndex = 0;
	bComboWindowOpen = false;
	bDamageConsumedForCurrentComboSection = false;
	bAuthorityAdvanceQueued = false;
	bAuthorityFallbackTimelineActive = ActorInfo && ActorInfo->IsNetAuthority()
		&& GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer;
	TestEventSource = bAuthorityFallbackTimelineActive
		? ECrunchComboEventSource::DedicatedFallback
		: ECrunchComboEventSource::Authored;
	bAuthorityFallbackEventContext = false;
	AuthorityFallbackEventSection = INDEX_NONE;
	OpenEventSectionMask = 0;
	DamageEventSectionMask = 0;
	CloseEventSectionMask = 0;
#if !UE_BUILD_SHIPPING
	TestOpenEventCount = 0;
	TestDamageEventCount = 0;
	TestCloseEventCount = 0;
	TestImplicitCloseEventCount = 0;
	TestAcceptedDamageCount = 0;
	TestAcceptedDamageSectionMask = 0;
	TestLastWindowOpenTime = -1.0;
	TestLastWindowCloseTime = -1.0;
#endif

	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ComboMontage);
		if (!MontageTask)
		{
			K2_EndAbility();
			return;
		}
		MontageTask->OnCancelled.AddDynamic(this, &UAuraMeleeAttack::K2_EndAbility);
		MontageTask->OnInterrupted.AddDynamic(this, &UAuraMeleeAttack::K2_EndAbility);
		// Dedicated fallback owns its terminal close and finish-grace timer;
		// binding normal montage completion here would end the replicated ability
		// before the remote client receives the final authored section.
		if (!bAuthorityFallbackTimelineActive)
		{
			MontageTask->OnBlendOut.AddDynamic(this, &UAuraMeleeAttack::K2_EndAbility);
			MontageTask->OnCompleted.AddDynamic(this, &UAuraMeleeAttack::K2_EndAbility);
		}
		MontageTask->ReadyForActivation();

		MontageJumpToSection(ComboSections[0]);

		DamageEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, DamageEventTag, nullptr, false, false);
		WindowOpenTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowOpenTag, nullptr, false, false);
		WindowCloseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ComboWindowCloseTag, nullptr, false, false);
		if (!DamageEventTask || !WindowOpenTask || !WindowCloseTask)
		{
			K2_EndAbility();
			return;
		}
		DamageEventTask->EventReceived.AddDynamic(this, &UAuraMeleeAttack::OnMontageEvent);
		WindowOpenTask->EventReceived.AddDynamic(this, &UAuraMeleeAttack::OnInputWindowOpened);
		WindowCloseTask->EventReceived.AddDynamic(this, &UAuraMeleeAttack::OnInputWindowClosed);
		DamageEventTask->ReadyForActivation();
		WindowOpenTask->ReadyForActivation();
		WindowCloseTask->ReadyForActivation();
		if (bAuthorityFallbackTimelineActive)
		{
			ArmAuthorityFallbackForSection(0);
		}
	}
}

void UAuraMeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// A replicated authority end can arrive before the predicted client's final
	// montage close notify. Treat an open local window as implicitly closed for
	// cleanup, but keep it out of authored notify accounting.
#if !UE_BUILD_SHIPPING
	if (bComboWindowOpen)
	{
		++TestImplicitCloseEventCount;
	}
#endif
	CleanupTasks();
	CurrentComboIndex = 0;
	bDamageConsumedForCurrentComboSection = false;
	bAuthorityAdvanceQueued = false;
	bAuthorityFallbackTimelineActive = false;
	bAuthorityFallbackEventContext = false;
	AuthorityFallbackEventSection = INDEX_NONE;
	OpenEventSectionMask = 0;
	DamageEventSectionMask = 0;
	CloseEventSectionMask = 0;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

#if !UE_BUILD_SHIPPING
bool UAuraMeleeAttack::HasTestAuthorityFallbackTimerPending() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const FTimerManager& TimerManager = World->GetTimerManager();
	return TimerManager.IsTimerActive(AuthorityFallbackOpenTimer)
		|| TimerManager.IsTimerActive(AuthorityFallbackDamageTimer)
		|| TimerManager.IsTimerActive(AuthorityFallbackCloseTimer)
		|| TimerManager.IsTimerActive(AuthorityFallbackFinishTimer);
}

const TCHAR* UAuraMeleeAttack::GetTestEventSourceName() const
{
	switch (TestEventSource)
	{
	case ECrunchComboEventSource::Authored:
		return TEXT("Authored");
	case ECrunchComboEventSource::DedicatedFallback:
		return TEXT("DedicatedFallback");
	default:
		return TEXT("Unknown");
	}
}
#endif

void UAuraMeleeAttack::OnMontageEvent(FGameplayEventData EventData)
{
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority()
		&& bAuthorityFallbackTimelineActive && !bAuthorityFallbackEventContext)
	{
		return;
	}
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		if (bAuthorityFallbackEventContext)
		{
			CurrentComboIndex = AuthorityFallbackEventSection;
		}
		else
		{
			SyncComboIndexToMontageSection();
		}
		if (CurrentComboIndex >= 0 && CurrentComboIndex < 32)
		{
			const uint32 SectionBit = (1u << CurrentComboIndex);
			if ((DamageEventSectionMask & SectionBit) != 0)
			{
				return;
			}
			DamageEventSectionMask |= SectionBit;
		}
#if !UE_BUILD_SHIPPING
		++TestDamageEventCount;
#endif
		if (!bDamageConsumedForCurrentComboSection)
		{
			bDamageConsumedForCurrentComboSection = true;
#if !UE_BUILD_SHIPPING
			++TestAcceptedDamageCount;
			if (CurrentComboIndex >= 0 && CurrentComboIndex < 32)
			{
				TestAcceptedDamageSectionMask |= (1 << CurrentComboIndex);
			}
#endif
			ApplyComboDamage();
		}
	}
}

void UAuraMeleeAttack::OnInputWindowOpened(FGameplayEventData EventData)
{
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority()
		&& bAuthorityFallbackTimelineActive && !bAuthorityFallbackEventContext)
	{
		return;
	}
	if (bAuthorityFallbackEventContext)
	{
		CurrentComboIndex = AuthorityFallbackEventSection;
	}
	else
	{
		SyncComboIndexToMontageSection();
	}
	if (CurrentComboIndex >= 0 && CurrentComboIndex < 32)
	{
		const uint32 SectionBit = (1u << CurrentComboIndex);
		if ((OpenEventSectionMask & SectionBit) != 0)
		{
			return;
		}
		OpenEventSectionMask |= SectionBit;
	}
	bComboWindowOpen = true;
#if !UE_BUILD_SHIPPING
	++TestOpenEventCount;
	TestLastWindowOpenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	TestLastWindowCloseTime = -1.0;
	UE_LOG(LogAura, Display,
		TEXT("[CrunchComboNetworkProbe][%s] AuthoredOpen Time=%.3f Section=%d PredictionKey=%d"),
		CurrentActorInfo && CurrentActorInfo->IsNetAuthority() ? TEXT("Server") : TEXT("Client"),
		TestLastWindowOpenTime, CurrentComboIndex, GetCurrentActivationInfo().GetActivationPredictionKey().Current);
#endif
	ArmInputPressTask();
}

void UAuraMeleeAttack::OnInputWindowClosed(FGameplayEventData EventData)
{
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority()
		&& bAuthorityFallbackTimelineActive && !bAuthorityFallbackEventContext)
	{
		return;
	}
	if (bAuthorityFallbackEventContext)
	{
		CurrentComboIndex = AuthorityFallbackEventSection;
	}
	if (CurrentComboIndex >= 0 && CurrentComboIndex < 32)
	{
		const uint32 SectionBit = (1u << CurrentComboIndex);
		if ((CloseEventSectionMask & SectionBit) == 0)
		{
			CloseEventSectionMask |= SectionBit;
		}
		else
		{
			return;
		}
	}
	bComboWindowOpen = false;
#if !UE_BUILD_SHIPPING
	++TestCloseEventCount;
	TestLastWindowCloseTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	UE_LOG(LogAura, Display,
		TEXT("[CrunchComboNetworkProbe][%s] AuthoredClose Time=%.3f Section=%d OpenTime=%.3f PredictionKey=%d"),
		CurrentActorInfo && CurrentActorInfo->IsNetAuthority() ? TEXT("Server") : TEXT("Client"),
		TestLastWindowCloseTime, CurrentComboIndex, TestLastWindowOpenTime,
		GetCurrentActivationInfo().GetActivationPredictionKey().Current);
#endif
	if (InputPressTask)
	{
		InputPressTask->EndTask();
		InputPressTask = nullptr;
	}
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && bAuthorityAdvanceQueued)
	{
		const int32 NextSectionIndex = CurrentComboIndex + 1;
		bAuthorityAdvanceQueued = false;
		if (NextSectionIndex < ComboSections.Num())
		{
			ArmAuthorityFallbackForSection(NextSectionIndex);
		}
	}
	else if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority() && bAuthorityFallbackTimelineActive)
	{
		// A dedicated server with no queued successor must not depend on
		// skeletal-mesh ticking to finish PlayMontageAndWait. Give the final
		// authoritative close a short replication grace so a remote client can
		// receive the last authored section/close before the server tears down
		// the replicated ability.
		if (CurrentComboIndex >= ComboSections.Num() - 1)
		{
			if (UWorld* World = GetWorld())
			{
				FTimerDelegate FinishDelegate;
				FinishDelegate.BindWeakLambda(this, [ThisObj = TWeakObjectPtr<UAuraMeleeAttack>(this)]()
				{
					if (UAuraMeleeAttack* Ability = ThisObj.Get())
					{
						Ability->K2_EndAbility();
					}
				});
				World->GetTimerManager().SetTimer(AuthorityFallbackFinishTimer, FinishDelegate, 0.75f, false);
			}
			else
			{
				K2_EndAbility();
			}
		}
		else
		{
			K2_EndAbility();
		}
	}
}

void UAuraMeleeAttack::ArmInputPressTask()
{
	if (!bComboWindowOpen || CurrentComboIndex >= ComboSections.Num() - 1 || InputPressTask)
	{
		return;
	}
	InputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	if (InputPressTask)
	{
		InputPressTask->OnPress.AddDynamic(this, &UAuraMeleeAttack::OnInputPressed);
		InputPressTask->ReadyForActivation();
	}
}

void UAuraMeleeAttack::OnInputPressed(float TimeWaited)
{
	InputPressTask = nullptr;

#if !UE_BUILD_SHIPPING
	const bool bIsAuthority = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
	const double DecisionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	const int32 PredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey().Current;
#endif
	if (!bComboWindowOpen || !ComboMontage || CurrentComboIndex >= ComboSections.Num() - 1)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogAura, Display,
			TEXT("[CrunchComboNetworkProbe][%s] InputDecision Time=%.3f Section=%d OpenTime=%.3f CloseTime=%.3f PredictionKey=%d Decision=Rejected Reason=WindowClosed"),
			bIsAuthority ? TEXT("Server") : TEXT("Client"), DecisionTime, CurrentComboIndex,
			TestLastWindowOpenTime, TestLastWindowCloseTime, PredictionKey);
#endif
		return;
	}

	USkeletalMeshComponent* Mesh = GetOwningComponentFromActorInfo();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	int32 SectionIndex = CurrentComboIndex;
	FName CurrentSection = ComboSections.IsValidIndex(SectionIndex) ? ComboSections[SectionIndex] : NAME_None;
	if (!(CurrentActorInfo && CurrentActorInfo->IsNetAuthority()))
	{
		if (!AnimInstance)
		{
			return;
		}
		CurrentSection = AnimInstance->Montage_GetCurrentSection(ComboMontage);
		SectionIndex = ComboSections.IndexOfByKey(CurrentSection);
	}
	if (SectionIndex == INDEX_NONE || SectionIndex >= ComboSections.Num() - 1)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogAura, Display,
			TEXT("[CrunchComboNetworkProbe][%s] InputDecision Time=%.3f Section=%d OpenTime=%.3f CloseTime=%.3f PredictionKey=%d Decision=Rejected Reason=SectionUnavailable"),
			bIsAuthority ? TEXT("Server") : TEXT("Client"), DecisionTime, SectionIndex,
			TestLastWindowOpenTime, TestLastWindowCloseTime, PredictionKey);
#endif
		return;
	}

	MontageSetNextSectionName(CurrentSection, ComboSections[SectionIndex + 1]);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogAura, Display,
		TEXT("[CrunchComboNetworkProbe][%s] InputDecision Time=%.3f Section=%d OpenTime=%.3f CloseTime=%.3f PredictionKey=%d Decision=Accepted NextSection=%d"),
		bIsAuthority ? TEXT("Server") : TEXT("Client"), DecisionTime, SectionIndex,
		TestLastWindowOpenTime, TestLastWindowCloseTime, PredictionKey, SectionIndex + 1);
#endif
	if (CurrentActorInfo && CurrentActorInfo->IsNetAuthority())
	{
		bAuthorityAdvanceQueued = true;
	}
	// Keep CurrentComboIndex tied to the section that is actually playing. The
	// montage can still emit this section's Damage notify after the input press;
	// the next Open/Damage event synchronizes the index when the section changes.
	ArmInputPressTask();
}

void UAuraMeleeAttack::ArmAuthorityFallbackForSection(const int32 SectionIndex)
{
	if (!CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !ComboMontage || !ComboSections.IsValidIndex(SectionIndex))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	float SectionStart = 0.f;
	float SectionEnd = 0.f;
	ComboMontage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
	const float SectionLength = FMath::Max(0.01f, SectionEnd - SectionStart);
	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(AuthorityFallbackOpenTimer);
	TimerManager.ClearTimer(AuthorityFallbackDamageTimer);
	TimerManager.ClearTimer(AuthorityFallbackCloseTimer);
	TimerManager.ClearTimer(AuthorityFallbackFinishTimer);

	FTimerDelegate OpenDelegate;
	OpenDelegate.BindWeakLambda(this, [ThisObj = TWeakObjectPtr<UAuraMeleeAttack>(this), SectionIndex]()
	{
		if (UAuraMeleeAttack* Ability = ThisObj.Get())
		{
			Ability->HandleAuthorityFallbackOpen(SectionIndex);
		}
	});
	FTimerDelegate DamageDelegate;
	DamageDelegate.BindWeakLambda(this, [ThisObj = TWeakObjectPtr<UAuraMeleeAttack>(this), SectionIndex]()
	{
		if (UAuraMeleeAttack* Ability = ThisObj.Get())
		{
			Ability->HandleAuthorityFallbackDamage(SectionIndex);
		}
	});
	FTimerDelegate CloseDelegate;
	CloseDelegate.BindWeakLambda(this, [ThisObj = TWeakObjectPtr<UAuraMeleeAttack>(this), SectionIndex]()
	{
		if (UAuraMeleeAttack* Ability = ThisObj.Get())
		{
			Ability->HandleAuthorityFallbackClose(SectionIndex);
		}
	});
	TimerManager.SetTimer(AuthorityFallbackOpenTimer, OpenDelegate, SectionLength * 0.35f, false);
	TimerManager.SetTimer(AuthorityFallbackDamageTimer, DamageDelegate, SectionLength * 0.55f, false);
	TimerManager.SetTimer(AuthorityFallbackCloseTimer, CloseDelegate, SectionLength * 0.75f, false);
}

void UAuraMeleeAttack::HandleAuthorityFallbackOpen(const int32 SectionIndex)
{
	if (!IsActive() || !ComboSections.IsValidIndex(SectionIndex))
	{
		return;
	}
	const uint32 SectionBit = 1u << SectionIndex;
	if ((OpenEventSectionMask & SectionBit) != 0)
	{
		return;
	}
	AuthorityFallbackEventSection = SectionIndex;
	bAuthorityFallbackEventContext = true;
	OnInputWindowOpened(FGameplayEventData());
	bAuthorityFallbackEventContext = false;
	AuthorityFallbackEventSection = INDEX_NONE;
}

void UAuraMeleeAttack::HandleAuthorityFallbackDamage(const int32 SectionIndex)
{
	if (!IsActive() || !ComboSections.IsValidIndex(SectionIndex))
	{
		return;
	}
	const uint32 SectionBit = 1u << SectionIndex;
	if ((DamageEventSectionMask & SectionBit) != 0)
	{
		return;
	}
	AuthorityFallbackEventSection = SectionIndex;
	bAuthorityFallbackEventContext = true;
	// The authority timeline owns one impact per section. Reset the per-section
	// guard unconditionally because the open callback has already moved the
	// logical section index before this timer fires.
	bDamageConsumedForCurrentComboSection = false;
	OnMontageEvent(FGameplayEventData());
	bAuthorityFallbackEventContext = false;
	AuthorityFallbackEventSection = INDEX_NONE;
}

void UAuraMeleeAttack::HandleAuthorityFallbackClose(const int32 SectionIndex)
{
	if (!IsActive() || !ComboSections.IsValidIndex(SectionIndex))
	{
		return;
	}
	AuthorityFallbackEventSection = SectionIndex;
	bAuthorityFallbackEventContext = true;
	OnInputWindowClosed(FGameplayEventData());
	bAuthorityFallbackEventContext = false;
	AuthorityFallbackEventSection = INDEX_NONE;
}

void UAuraMeleeAttack::SyncComboIndexToMontageSection()
{
	if (!ComboMontage)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = GetOwningComponentFromActorInfo();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}
	const int32 SectionIndex = ComboSections.IndexOfByKey(AnimInstance->Montage_GetCurrentSection(ComboMontage));
	if (SectionIndex != INDEX_NONE && SectionIndex != CurrentComboIndex)
	{
		CurrentComboIndex = SectionIndex;
		bDamageConsumedForCurrentComboSection = false;
	}
}

void UAuraMeleeAttack::ApplyComboDamage()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority())
	{
		return;
	}

	TArray<AActor*> Targets;
	UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(Avatar, Targets, { Avatar }, DamageRadius, Avatar->GetActorLocation());
	TSet<AActor*> UniqueTargets;
	for (AActor* Target : Targets)
	{
		if (!IsValid(Target) || Target == Avatar || UniqueTargets.Contains(Target))
		{
			continue;
		}
		UniqueTargets.Add(Target);
		FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
		if (Params.BaseDamage <= 0.f)
		{
			Params.BaseDamage = DefaultComboDamage * FMath::Max(1, GetAbilityLevel());
		}
		if (!Params.DamageType.IsValid())
		{
			Params.DamageType = FGameplayTag::RequestGameplayTag(TEXT("Damage.Physical"), false);
		}
		const FGameplayEffectContextHandle DamageContext = UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
		if (!DamageContext.IsValid())
		{
			UE_LOG(LogAura, Verbose, TEXT("[CrunchCombo] Damage rejected; impact cue skipped Target=%s"), *GetNameSafe(Target));
			continue;
		}

		// Damage is authoritative, so the target ASC is the replication boundary for
		// the impact cue. EffectCauser is the hit actor because GC_MeleeImpact uses it
		// to resolve the target's blood effect; Instigator remains the attacking avatar.
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Target->GetActorLocation();
			CueParams.EffectCauser = Target;
			CueParams.Instigator = Avatar;
			CueParams.SourceObject = Avatar;
			TargetASC->ExecuteGameplayCue(FAuraGameplayTags::Get().GameplayCue_MeleeImpact, CueParams);
			UE_LOG(LogAura, Log, TEXT("[CrunchCombo] Broadcast gameplay cue tag=%s Target=%s Location=%s via target ASC"),
				*FAuraGameplayTags::Get().GameplayCue_MeleeImpact.ToString(),
				*GetNameSafe(Target),
				*CueParams.Location.ToCompactString());
		}
		else
		{
			UE_LOG(LogAura, Warning, TEXT("[CrunchCombo] Cannot broadcast gameplay cue: target ASC is null Target=%s"), *GetNameSafe(Target));
		}
	}
}

void UAuraMeleeAttack::CleanupTasks()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AuthorityFallbackOpenTimer);
		World->GetTimerManager().ClearTimer(AuthorityFallbackDamageTimer);
		World->GetTimerManager().ClearTimer(AuthorityFallbackCloseTimer);
		World->GetTimerManager().ClearTimer(AuthorityFallbackFinishTimer);
	}
	bComboWindowOpen = false;
	if (InputPressTask) InputPressTask->EndTask();
	if (DamageEventTask) DamageEventTask->EndTask();
	if (WindowOpenTask) WindowOpenTask->EndTask();
	if (WindowCloseTask) WindowCloseTask->EndTask();
	InputPressTask = nullptr;
	DamageEventTask = nullptr;
	WindowOpenTask = nullptr;
	WindowCloseTask = nullptr;
	MontageTask = nullptr;
}
