// Copyright Druid Mechanics

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/Abilities/AuraMeleeAttack.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AuraGameplayTags.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "Combat/AuraCombatStateComponent.h"
#include "GameplayTagsManager.h"
#include "Tests/Fixtures/AuraRoleApplicationTestActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchComboTagRegistrationTest,
	"Aura.Migration.Crunch.TagRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchComboTagRegistrationTest::RunTest(const FString& Parameters)
{
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const FGameplayTag ConfigCueTag = UGameplayTagsManager::Get().RequestGameplayTag(
		FName(TEXT("GameplayCue.MeleeImpact")), false);
	TestTrue(TEXT("Config-owned melee impact gameplay cue tag is registered"), ConfigCueTag.IsValid());
	TestTrue(TEXT("Native accessor resolves the config-owned melee impact tag"), Tags.GameplayCue_MeleeImpact.IsValid());
	TestEqual(TEXT("Native and config cue tags resolve to the same tag"), Tags.GameplayCue_MeleeImpact, ConfigCueTag);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchComboImpactCueDispatchBehaviorTest,
	"Aura.Migration.Crunch.ImpactCueDispatchBehavior",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchComboImpactCueDispatchBehaviorTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient cue dispatch world created"), TestWorld))
	{
		return false;
	}
	TestWorld->AddToRoot();
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAuraRoleApplicationTestActor* Target = TestWorld->SpawnActor<AAuraRoleApplicationTestActor>(
		AAuraRoleApplicationTestActor::StaticClass(), FTransform::Identity, SpawnParameters);
	const bool bSpawned = TestNotNull(TEXT("Cue target fixture spawned"), Target);
	if (bSpawned)
	{
		TestTrue(TEXT("Cue target ASC initializes actor info"), Target->InitializeTestAbilityActorInfo());
		Target->DispatchBeginPlay();
		TestWorld->BeginPlay();
		UAuraAbilitySystemComponent* TargetASC = Target->GetTestASC();
		TestNotNull(TEXT("Cue target owns an Aura ASC"), TargetASC);
		if (TargetASC)
		{
			const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
			const FVector ExpectedLocation(40.f, -20.f, 15.f);
			FGameplayCueParameters CueParams;
			CueParams.Location = ExpectedLocation;
			CueParams.EffectCauser = Target;
			CueParams.Instigator = Target;
			CueParams.SourceObject = Target;
			Target->ResetGameplayCueProbe();
			TargetASC->ExecuteGameplayCue(Tags.GameplayCue_MeleeImpact, CueParams);
			TestEqual(TEXT("One gameplay cue dispatch reaches the target listener"), Target->GetGameplayCueProbeCount(), 1);
			TestEqual(TEXT("Cue listener receives the impact tag"), Target->GetLastGameplayCueTag(), Tags.GameplayCue_MeleeImpact);
			TestTrue(TEXT("Cue listener receives the authored target location"),
				Target->GetLastGameplayCueLocation().Equals(ExpectedLocation, 0.1f));
			TestTrue(TEXT("Cue listener receives the target effect causer"), Target->GetLastGameplayCueEffectCauser() == Target);
			TestTrue(TEXT("Cue listener receives the attacking instigator"), Target->GetLastGameplayCueInstigator() == Target);
			TestTrue(TEXT("Cue listener receives the source object"), Target->GetLastGameplayCueSourceObject() == Target);
		}
	}

	if (Target)
	{
		Target->Destroy();
	}
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->RemoveFromRoot();
	TestWorld->DestroyWorld(false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchComboActivationPermissionFailureTest,
	"Aura.Migration.Crunch.ActivationPermissionFailureCleansUp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchComboActivationPermissionFailureTest::RunTest(const FString& Parameters)
{
	UAuraMeleeAttack* Ability = NewObject<UAuraMeleeAttack>();
	if (!TestNotNull(TEXT("Transient combo ability created for permission failure"), Ability))
	{
		return false;
	}

	FGameplayAbilityActivationInfo ActivationInfo;
	Ability->ActivateAbility(FGameplayAbilitySpecHandle(), nullptr, ActivationInfo, nullptr);
	TestFalse(TEXT("Null actor info fails closed without activating the combo"), Ability->IsActive());

	FGameplayAbilityActorInfo EmptyActorInfo;
	Ability->ActivateAbility(FGameplayAbilitySpecHandle(), &EmptyActorInfo, ActivationInfo, nullptr);
	TestFalse(TEXT("Actor info without an ASC fails closed without activating the combo"), Ability->IsActive());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAuraCrunchComboRuntimePlaybackTest,
	"Aura.Migration.CrunchCombo.RuntimePlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAuraCrunchComboRuntimePlaybackTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Transient combo runtime world created"), TestWorld))
	{
		return false;
	}
	TestWorld->AddToRoot();
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(TestWorld);
	TestWorld->InitializeActorsForPlay(FURL());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAuraRoleApplicationTestActor* Avatar = TestWorld->SpawnActor<AAuraRoleApplicationTestActor>(
		AAuraRoleApplicationTestActor::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("Combo runtime avatar spawned"), Avatar))
	{
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(false);
		return false;
	}

	UAnimMontage* ComboMontage = LoadObject<UAnimMontage>(
		nullptr,
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_CrunchComboV4.AM_CrunchComboV4"));
	UClass* AuraAnimClass = LoadClass<UAnimInstance>(
		nullptr,
		TEXT("/Game/Blueprints/Character/Crunch/ABP_Crunch_AuraV5.ABP_Crunch_AuraV5_C"));
	USkeletalMesh* AuraMesh = LoadObject<USkeletalMesh>(
		nullptr, TEXT("/Game/Assets/Characters/Crunch/Meshes/SM_CrunchV4.SM_CrunchV4"));
	if (!TestNotNull(TEXT("Generated combo montage loads for playback"), ComboMontage)
		|| !TestNotNull(TEXT("Aura animation blueprint loads for playback"), AuraAnimClass)
		|| !TestNotNull(TEXT("Aura skeleton mesh loads for playback"), AuraMesh))
	{
		Avatar->Destroy();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(false);
		return false;
	}
	// GAS creates InstancedPerActor abilities with NewObject<Class>(), not as CDO
	// clones, so keep the target montage explicit for this runtime fixture.
	UAuraMeleeAttack::SetTestComboMontageOverride(ComboMontage);
	UE_LOG(LogTemp, Display, TEXT("[CrunchComboRuntimeTest] Test montage=%s skeleton=%s"),
		*GetNameSafe(ComboMontage), *GetNameSafe(ComboMontage->GetSkeleton()));

	const FGameplayTag EntityPlayer = FGameplayTag::RequestGameplayTag(TEXT("Entity.Player"));
	const FGameplayTag ControlPlayer = FGameplayTag::RequestGameplayTag(TEXT("Control.Player"));
	Avatar->ConfigureRoleShell(EntityPlayer, ControlPlayer);
	Avatar->GetMesh()->SetSkeletalMeshAsset(AuraMesh);
	Avatar->GetMesh()->SetAnimInstanceClass(AuraAnimClass);
	Avatar->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Avatar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Avatar->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	// The transient automation world is advanced manually below. Disable the
	// component's normal tick so each frame updates the montage exactly once.
	Avatar->GetMesh()->SetComponentTickEnabled(false);
	Avatar->InitializeTestAbilityActorInfo();
	Avatar->DispatchBeginPlay();
	TestWorld->BeginPlay();
	Avatar->GetCombatStateComponentMutable()->TryEnterAlive();

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	TArray<AAuraRoleApplicationTestActor*> TargetFixtures;
	auto SpawnTarget = [&](const FVector& Location, const FAuraCombatIdentity& Identity, bool bDead)
		-> AAuraRoleApplicationTestActor*
	{
		AAuraRoleApplicationTestActor* Target = TestWorld->SpawnActor<AAuraRoleApplicationTestActor>(
			AAuraRoleApplicationTestActor::StaticClass(), FTransform(Location), SpawnParameters);
		if (!Target)
		{
			return nullptr;
		}
		Target->SetActorLocation(Location);
		Target->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Target->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
		Target->GetCapsuleComponent()->SetGenerateOverlapEvents(true);
		const bool bAbilityInfoReady = Target->InitializeTestAbilityActorInfo();
		Target->InitializeCombatIdentityForTest(Identity);
		if (UAuraAbilitySystemComponent* TargetASC = Target->GetTestASC())
		{
			TargetASC->SetNumericAttributeBase(UAuraAttributeSet::GetMaxHealthAttribute(), 100.f);
			TargetASC->SetNumericAttributeBase(UAuraAttributeSet::GetHealthAttribute(), 100.f);
		}
		if (!bAbilityInfoReady)
		{
			Target->Destroy();
			return nullptr;
		}
		if (bDead)
		{
			Target->GetCombatStateComponentMutable()->TryEnterDying();
		}
		TargetFixtures.Add(Target);
		return Target;
	};

	FAuraCombatIdentity HostileIdentity;
	HostileIdentity.FactionTag = Tags.Faction_Enemy;
	HostileIdentity.ControlTypeTag = Tags.Control_EnemyAI;
	HostileIdentity.CombatProfileTag = Tags.Combat_Unassigned;
	HostileIdentity.DeathPolicyTag = Tags.Death_EnemyLoot;
	HostileIdentity.bTargetable = true;
	HostileIdentity.bCanAttack = true;
	HostileIdentity.bCanBeDamaged = true;
	FAuraCombatIdentity FriendlyIdentity;
	FriendlyIdentity.FactionTag = Tags.Faction_Player;
	FriendlyIdentity.ControlTypeTag = Tags.Control_Player;
	FriendlyIdentity.CombatProfileTag = Tags.Combat_Unassigned;
	FriendlyIdentity.DeathPolicyTag = Tags.Death_PlayerRespawn;
	FriendlyIdentity.bTargetable = true;
	FriendlyIdentity.bCanAttack = true;
	FriendlyIdentity.bCanBeDamaged = true;
	AAuraRoleApplicationTestActor* HostileTarget = SpawnTarget(FVector(100.f, 0.f, 0.f), HostileIdentity, false);
	AAuraRoleApplicationTestActor* FriendlyTarget = SpawnTarget(FVector(100.f, 100.f, 0.f), FriendlyIdentity, false);
	AAuraRoleApplicationTestActor* DeadTarget = SpawnTarget(FVector(100.f, -100.f, 0.f), HostileIdentity, true);
	AAuraRoleApplicationTestActor* OutsideTarget = SpawnTarget(FVector(500.f, 0.f, 0.f), HostileIdentity, false);
	TestNotNull(TEXT("Hostile target fixture spawned"), HostileTarget);
	TestNotNull(TEXT("Friendly target fixture spawned"), FriendlyTarget);
	TestNotNull(TEXT("Dead target fixture spawned"), DeadTarget);
	TestNotNull(TEXT("Outside-radius target fixture spawned"), OutsideTarget);
	TArray<AActor*> InitialTargets;
	UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(Avatar, InitialTargets, { Avatar }, 150.f, Avatar->GetActorLocation());
	UE_LOG(LogTemp, Display, TEXT("[CrunchComboRuntimeTest] Initial target query count=%d Hostile=%d Friendly=%d Dead=%d Outside=%d Locations=(%.1f,%.1f)/(%.1f,%.1f)/(%.1f,%.1f)/(%.1f,%.1f)"),
		InitialTargets.Num(), InitialTargets.Contains(HostileTarget) ? 1 : 0, InitialTargets.Contains(FriendlyTarget) ? 1 : 0,
		InitialTargets.Contains(DeadTarget) ? 1 : 0, InitialTargets.Contains(OutsideTarget) ? 1 : 0,
		HostileTarget ? HostileTarget->GetActorLocation().X : -1.f, HostileTarget ? HostileTarget->GetActorLocation().Y : -1.f,
		FriendlyTarget ? FriendlyTarget->GetActorLocation().X : -1.f, FriendlyTarget ? FriendlyTarget->GetActorLocation().Y : -1.f,
		DeadTarget ? DeadTarget->GetActorLocation().X : -1.f, DeadTarget ? DeadTarget->GetActorLocation().Y : -1.f,
		OutsideTarget ? OutsideTarget->GetActorLocation().X : -1.f, OutsideTarget ? OutsideTarget->GetActorLocation().Y : -1.f);

	UAuraAbilitySystemComponent* ASC = Avatar->GetTestASC();
	if (!TestNotNull(TEXT("Combo runtime avatar owns an Aura ASC"), ASC))
	{
		Avatar->Destroy();
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->RemoveFromRoot();
		TestWorld->DestroyWorld(false);
		return false;
	}

	FGameplayAbilitySpec Spec(UAuraMeleeAttack::StaticClass(), 1);
	Spec.GetDynamicSpecSourceTags().AddTag(Tags.InputTag_LMB);
	Spec.GetDynamicSpecSourceTags().AddTag(Tags.Abilities_Status_Equipped);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	TestTrue(TEXT("Combo runtime spec granted"), Handle.IsValid());

	ASC->AbilityInputTagPressed(Tags.InputTag_LMB);
	ASC->AbilityInputTagHeld(Tags.InputTag_LMB);
	const FGameplayAbilitySpec* LiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Combo activates from the Aura LMB input path"), LiveSpec && LiveSpec->IsActive());
	UAuraMeleeAttack* AbilityInstance = LiveSpec ? Cast<UAuraMeleeAttack>(LiveSpec->GetPrimaryInstance()) : nullptr;
	TestNotNull(TEXT("Combo activation creates an instanced native ability"), AbilityInstance);

	UAnimInstance* AnimInstance = Avatar->GetMesh()->GetAnimInstance();
	TestNotNull(TEXT("Aura avatar owns an animation instance"), AnimInstance);
	if (AnimInstance)
	{
		TestTrue(TEXT("Combo montage starts on activation"), AnimInstance->Montage_IsPlaying(ComboMontage));
		TestEqual(TEXT("Combo starts in Combo01"), AnimInstance->Montage_GetCurrentSection(ComboMontage), FName(TEXT("Combo01")));
	}

	for (int32 TickIndex = 0; TickIndex < 40; ++TickIndex)
	{
		TestWorld->Tick(LEVELTICK_All, 0.1f);
		// Isolated automation worlds do not always schedule skeletal component
		// ticks, so drive the animation tick explicitly as well as the world clock.
		// Dispatching queued events mirrors the normal skeletal component tick;
		// TickAnimation alone only queues montage notifies.
		if (Avatar->GetMesh()->ShouldTickAnimation())
		{
			Avatar->GetMesh()->TickAnimation(0.1f, false);
			Avatar->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[CrunchComboRuntimeTest] WorldTime=%.3f MontagePlaying=%d"),
		TestWorld->GetTimeSeconds(), AnimInstance && AnimInstance->Montage_IsPlaying(ComboMontage) ? 1 : 0);
	if (AnimInstance)
	{
		UE_LOG(LogTemp, Display, TEXT("[CrunchComboRuntimeTest] Section=%s Position=%.3f Length=%.3f"),
			*AnimInstance->Montage_GetCurrentSection(ComboMontage).ToString(),
			AnimInstance->Montage_GetPosition(ComboMontage), ComboMontage->GetPlayLength());
	}
	LiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	if (AnimInstance)
	{
		TestFalse(TEXT("Combo montage stops after Combo01 natural completion"), AnimInstance->Montage_IsPlaying(ComboMontage));
	}
	TestTrue(TEXT("Combo spec remains addressable after montage playback"), LiveSpec != nullptr);
	if (AbilityInstance)
	{
		UE_LOG(LogTemp, Display, TEXT("[CrunchComboRuntimeTest] Active montage=%s expected=%s"),
			*GetNameSafe(AbilityInstance->GetCurrentMontage()), *GetNameSafe(ComboMontage));
		TestEqual(TEXT("Standalone combo uses authored montage events"), FString(AbilityInstance->GetTestEventSourceName()), FString(TEXT("Authored")));
		TestEqual(TEXT("Combo01 emits one Window.Open event"), AbilityInstance->GetTestOpenEventCount(), 1);
		TestEqual(TEXT("Combo01 emits one Damage event"), AbilityInstance->GetTestDamageEventCount(), 1);
		TestEqual(TEXT("Combo01 emits one Window.Close event"), AbilityInstance->GetTestCloseEventCount(), 1);
		TestEqual(TEXT("Combo01 authored close accounting excludes teardown"), AbilityInstance->GetTestImplicitCloseEventCount(), 0);
		TestEqual(TEXT("Combo01 accepts one authoritative damage callback"), AbilityInstance->GetTestAcceptedDamageCount(), 1);
	}
	const float HostileHealthAfterOneSection = HostileTarget && HostileTarget->GetTestASC()
		? HostileTarget->GetTestASC()->GetNumericAttribute(UAuraAttributeSet::GetHealthAttribute()) : -1.f;
	const float FriendlyHealthAfterOneSection = FriendlyTarget && FriendlyTarget->GetTestASC()
		? FriendlyTarget->GetTestASC()->GetNumericAttribute(UAuraAttributeSet::GetHealthAttribute()) : -1.f;
	const float DeadHealthAfterOneSection = DeadTarget && DeadTarget->GetTestASC()
		? DeadTarget->GetTestASC()->GetNumericAttribute(UAuraAttributeSet::GetHealthAttribute()) : -1.f;
	const float OutsideHealthAfterOneSection = OutsideTarget && OutsideTarget->GetTestASC()
		? OutsideTarget->GetTestASC()->GetNumericAttribute(UAuraAttributeSet::GetHealthAttribute()) : -1.f;
	TestTrue(TEXT("Authoritative combo damage reaches one live hostile target"), HostileHealthAfterOneSection < 100.f);
	TestEqual(TEXT("Accepted hostile hit dispatches exactly one impact cue"),
		HostileTarget ? HostileTarget->GetGameplayCueProbeCount() : -1, 1);
	TestEqual(TEXT("Impact cue reaches the hostile target ASC listener"),
		HostileTarget ? HostileTarget->GetLastGameplayCueTag() : FGameplayTag(), Tags.GameplayCue_MeleeImpact);
	TestTrue(TEXT("Impact cue location matches the hostile target"), HostileTarget
		&& HostileTarget->GetLastGameplayCueLocation().Equals(HostileTarget->GetActorLocation(), 0.1f));
	TestTrue(TEXT("Impact cue effect causer is the hostile target"), HostileTarget
		&& HostileTarget->GetLastGameplayCueEffectCauser() == HostileTarget);
	TestTrue(TEXT("Impact cue instigator is the attacking avatar"), HostileTarget
		&& HostileTarget->GetLastGameplayCueInstigator() == Avatar);
	TestTrue(TEXT("Impact cue source object is the attacking avatar"), HostileTarget
		&& HostileTarget->GetLastGameplayCueSourceObject() == Avatar);
	TestEqual(TEXT("Friendly target is denied by the combat policy"), FriendlyHealthAfterOneSection, 100.f);
	TestEqual(TEXT("Dead target is excluded before damage application"), DeadHealthAfterOneSection, 100.f);
	TestEqual(TEXT("Outside-radius target is excluded before damage application"), OutsideHealthAfterOneSection, 100.f);
	ASC->CancelAbilityHandle(Handle);
	LiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Combo cancellation releases the active spec"), LiveSpec && !LiveSpec->IsActive());

	// Queue a successor and cancel before the first section can reach Damage.
	// This proves pending MontageSetNextSectionName state and WaitInputPress
	// cleanup do not leak through cancellation.
	ASC->AbilityInputTagReleased(Tags.InputTag_LMB);
	ASC->AbilityInputTagPressed(Tags.InputTag_LMB);
	ASC->AbilityInputTagHeld(Tags.InputTag_LMB);
	LiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	AbilityInstance = LiveSpec ? Cast<UAuraMeleeAttack>(LiveSpec->GetPrimaryInstance()) : nullptr;
	TestTrue(TEXT("Combo can reactivate for queued-cancel coverage"), LiveSpec && LiveSpec->IsActive() && AbilityInstance != nullptr);
	bool bQueuedThenCancelled = false;
	for (int32 TickIndex = 0; TickIndex < 8 && !bQueuedThenCancelled; ++TickIndex)
	{
		TestWorld->Tick(LEVELTICK_All, 0.1f);
		if (Avatar->GetMesh()->ShouldTickAnimation())
		{
			Avatar->GetMesh()->TickAnimation(0.1f, false);
			Avatar->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
		}
		if (AbilityInstance && AbilityInstance->IsTestComboWindowOpen())
		{
			ASC->AbilityInputTagPressed(Tags.InputTag_LMB);
			ASC->CancelAbilityHandle(Handle);
			bQueuedThenCancelled = true;
		}
	}
	TestTrue(TEXT("Queued successor input is observed before cancellation"), bQueuedThenCancelled);
	for (int32 TickIndex = 0; TickIndex < 12; ++TickIndex)
	{
		TestWorld->Tick(LEVELTICK_All, 0.1f);
		if (Avatar->GetMesh()->ShouldTickAnimation())
		{
			Avatar->GetMesh()->TickAnimation(0.1f, false);
			Avatar->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
		}
	}
	LiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Queued cancellation leaves the combo inactive"), LiveSpec && !LiveSpec->IsActive());
	if (AnimInstance)
	{
		TestFalse(TEXT("Queued cancellation stops the montage"), AnimInstance->Montage_IsPlaying(ComboMontage));
	}
	if (AbilityInstance)
	{
		TestEqual(TEXT("Queued cancellation produces no delayed damage event"), AbilityInstance->GetTestDamageEventCount(), 0);
		TestEqual(TEXT("Queued cancellation accepts no damage"), AbilityInstance->GetTestAcceptedDamageCount(), 0);
		TestEqual(TEXT("Queued cancellation keeps authored close count at zero"), AbilityInstance->GetTestCloseEventCount(), 0);
		TestEqual(TEXT("Queued cancellation records one implicit teardown close"), AbilityInstance->GetTestImplicitCloseEventCount(), 1);
	}

	// Reactivate and press once per open window. This proves the callback-driven
	// section queue without relying on a static montage successor.
	ASC->AbilityInputTagReleased(Tags.InputTag_LMB);
	ASC->AbilityInputTagPressed(Tags.InputTag_LMB);
	ASC->AbilityInputTagHeld(Tags.InputTag_LMB);
	LiveSpec = ASC->FindAbilitySpecFromHandle(Handle);
	AbilityInstance = LiveSpec ? Cast<UAuraMeleeAttack>(LiveSpec->GetPrimaryInstance()) : nullptr;
	TestTrue(TEXT("Combo can reactivate after queued cancellation"), LiveSpec && LiveSpec->IsActive() && AbilityInstance != nullptr);
	int32 LastOpenEventCount = 0;
	int32 PressesAccepted = 0;
	int32 MaxObservedComboIndex = 0;
	for (int32 TickIndex = 0; TickIndex < 70; ++TickIndex)
	{
		TestWorld->Tick(LEVELTICK_All, 0.1f);
		if (Avatar->GetMesh()->ShouldTickAnimation())
		{
			Avatar->GetMesh()->TickAnimation(0.1f, false);
			Avatar->GetMesh()->ConditionallyDispatchQueuedAnimEvents();
		}
		if (AbilityInstance)
		{
			MaxObservedComboIndex = FMath::Max(MaxObservedComboIndex, AbilityInstance->GetTestComboIndex());
			if (AbilityInstance->IsTestComboWindowOpen()
				&& AbilityInstance->GetTestOpenEventCount() > LastOpenEventCount
				&& AbilityInstance->GetTestComboIndex() < 3)
			{
				LastOpenEventCount = AbilityInstance->GetTestOpenEventCount();
				ASC->AbilityInputTagPressed(Tags.InputTag_LMB);
				++PressesAccepted;
			}
		}
	}
	if (AbilityInstance)
	{
		TestEqual(TEXT("Timed input reaches all four combo windows"), AbilityInstance->GetTestOpenEventCount(), 4);
		TestEqual(TEXT("Timed input receives four damage events"), AbilityInstance->GetTestDamageEventCount(), 4);
		TestEqual(TEXT("Timed input receives four close events"), AbilityInstance->GetTestCloseEventCount(), 4);
		TestEqual(TEXT("Timed input has no implicit teardown close"), AbilityInstance->GetTestImplicitCloseEventCount(), 0);
		TestEqual(TEXT("Timed input accepts three section transitions"), PressesAccepted, 3);
		TestEqual(TEXT("Timed input reaches Combo04"), MaxObservedComboIndex, 3);
		TestEqual(TEXT("Timed input applies damage once per section"), AbilityInstance->GetTestAcceptedDamageCount(), 4);
		TestEqual(TEXT("Input-before-damage ordering preserves one accepted hit in every section"),
			AbilityInstance->GetTestAcceptedDamageSectionMask(), 0xF);
	}
	ASC->CancelAbilityHandle(Handle);
	UAuraMeleeAttack::SetTestComboMontageOverride(nullptr);

	Avatar->Destroy();
	for (AAuraRoleApplicationTestActor* Target : TargetFixtures)
	{
		if (Target)
		{
			Target->Destroy();
		}
	}
	GEngine->DestroyWorldContext(TestWorld);
	TestWorld->RemoveFromRoot();
	TestWorld->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif
