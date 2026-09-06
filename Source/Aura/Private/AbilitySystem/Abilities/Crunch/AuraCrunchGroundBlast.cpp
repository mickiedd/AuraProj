// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchGroundBlast.h"

#include "AbilitySystem/Abilities/Crunch/AuraCrunchTagUtils.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimMontage.h"

UAuraCrunchGroundBlast::UAuraCrunchGroundBlast()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(AuraCrunchTags::Request(TEXT("Abilities.Melee.CrunchGroundBlast")));
	SetAssetTags(AssetTags);
	StartupInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.3")), false);
	CrunchStartupInputTagName = FName(TEXT("InputTag.3"));
	CrunchCooldownTag = AuraCrunchTags::Request(TEXT("Cooldown.Melee.CrunchGroundBlast"));
	CrunchManaCost = 0.f;
	CrunchCooldown = 0.f;
	DefaultCrunchDamage = 45.f;
	// The legacy Paragon montages reference an animation absent from this project.
	// This immediate-cast path does not need a targeting montage.
	static ConstructorHelpers::FObjectFinder<UAnimMontage> CastFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_GroundBlast_Aura.AM_GroundBlast_Aura"));
	if (CastFinder.Succeeded()) CastMontage = CastFinder.Object;
}

void UAuraCrunchGroundBlast::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!BeginCrunchActivation(ActorInfo, ActivationInfo))
	{
		K2_EndAbility();
		return;
	}
	bCommitted = false;

	// Arm the deadline before either ability task is activated.  Target data can be
	// delivered synchronously when the replicated prediction key is already present,
	// so registering it afterwards creates a short cleanup race.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TargetTimeoutTimer, this, &UAuraCrunchGroundBlast::HandleTargetTimeout, 5.f, false);
		RegisterCrunchTimer(TargetTimeoutTimer);
	}

	// GroundBlast deliberately commits after the server retrace. A cancelled or forged
	// target therefore consumes neither mana nor cooldown.
	if (TargetingMontage)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TargetingMontage);
		if (MontageTask)
		{
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchGroundBlast::K2_EndAbility);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchGroundBlast::K2_EndAbility);
			MontageTask->ReadyForActivation();
		}
	}
	// Montage activation can synchronously cancel this ability.
	if (!IsActive()) return;
	TargetDataTask = UTargetDataUnderMouse::CreateTargetDataUnderMouse(this);
	if (!TargetDataTask)
	{
		K2_EndAbility();
		return;
	}
	TargetDataTask->ValidData.AddDynamic(this, &UAuraCrunchGroundBlast::HandleTargetData);
	TargetDataTask->ReadyForActivation();
}

void UAuraCrunchGroundBlast::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogAura, Log, TEXT("[CrunchGroundBlast] End authority=%d committed=%d cancelled=%d"),
		ActorInfo && ActorInfo->IsNetAuthority(), bCommitted, bWasCancelled);
	if (TargetDataTask) TargetDataTask->EndTask();
	if (MontageTask) MontageTask->EndTask();
	TargetDataTask = nullptr;
	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraCrunchGroundBlast::HandleTargetData(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	if (!IsActive() || bCommitted || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority())
	{
		return;
	}
	if (DataHandle.Num() != 1 || !DataHandle.Get(0) || !DataHandle.Get(0)->GetHitResult())
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchGroundBlast] Rejected malformed target data"));
		K2_EndAbility();
		return;
	}
	const FHitResult Hit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(DataHandle, 0);
	FVector Point;
	if (!ValidateGroundTarget(Hit, Point))
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchGroundBlast] Rejected target=%s blocking=%d: range/LOS/surface validation failed."),
			*Hit.ImpactPoint.ToString(), Hit.bBlockingHit);
		K2_EndAbility();
		return;
	}
	if (!K2_CommitAbility())
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchGroundBlast] Commit denied by cost/cooldown"));
		K2_EndAbility();
		return;
	}
	CommitGroundBlast(Point);
}

void UAuraCrunchGroundBlast::HandleTargetTimeout()
{
	if (!bCommitted)
	{
		UE_LOG(LogAura, Warning, TEXT("[CrunchGroundBlast] Target data timed out"));
		K2_EndAbility();
	}
}

void UAuraCrunchGroundBlast::HandleCastFinished()
{
	if (IsActive()) K2_EndAbility();
}

bool UAuraCrunchGroundBlast::ValidateGroundTarget(const FHitResult& HitResult, FVector& OutPoint) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority() || !GetWorld() || !HitResult.bBlockingHit || !FMath::IsFinite(HitResult.ImpactPoint.X)
		|| !FMath::IsFinite(HitResult.ImpactPoint.Y) || !FMath::IsFinite(HitResult.ImpactPoint.Z))
	{
		return false;
	}
	const FVector Origin = Avatar->GetActorLocation();
	FVector Point = HitResult.ImpactPoint;
	FVector FlatDelta = Point - Origin;
	FlatDelta.Z = 0.f;
	if (FlatDelta.SizeSquared() > FMath::Square(TargetTraceRange))
	{
		// Clamp only the horizontal component.  The previous implementation rebuilt
		// the complete point from the caster, which silently discarded the requested
		// elevation before the height check.
		Point = Origin + FlatDelta.GetSafeNormal() * TargetTraceRange;
		Point.Z = HitResult.ImpactPoint.Z;
	}
	if (FMath::Abs(Point.Z - Origin.Z) > MaxTargetHeight)
	{
		return false;
	}
	FHitResult GroundHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrunchGroundBlastRetrace), false);
	Params.AddIgnoredActor(Avatar);
	const FVector TraceStart = Point + FVector::UpVector * 100.f;
	const FVector TraceEnd = Point - FVector::UpVector * 250.f;
	if (!GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params)
		|| GroundHit.ImpactNormal.Z < 0.5f)
	{
		return false;
	}
	OutPoint = GroundHit.ImpactPoint;
	if (FMath::Abs(OutPoint.Z - Origin.Z) > MaxTargetHeight)
	{
		return false;
	}

	// A valid ground hit is not sufficient when a wall or closed volume blocks the
	// caster.  Probe slightly above the surface so the supporting floor is allowed
	// to be the terminal hit, while an earlier obstruction rejects the request.
	FHitResult LOSHit;
	const FVector LOSStart = Origin + FVector::UpVector * 50.f;
	const FVector LOSEnd = OutPoint + FVector::UpVector * 5.f;
	if (GetWorld()->LineTraceSingleByChannel(LOSHit, LOSStart, LOSEnd, ECC_Visibility, Params)
		&& (LOSHit.ImpactPoint - LOSEnd).SizeSquared() > FMath::Square(10.f))
	{
		return false;
	}
	return true;
}

void UAuraCrunchGroundBlast::CommitGroundBlast(const FVector& Point)
{
	if (bCommitted) return;
	bCommitted = true;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;
	TArray<AActor*> Targets;
	// The center was already range-validated. Victim membership is relative to the
	// accepted blast center; applying the caster range again would drop valid edge
	// victims when the center is near the maximum cast distance.
	CollectCrunchTargets(Point, TargetAreaRadius, Targets, 0.f);
	UE_LOG(LogAura, Log, TEXT("[CrunchGroundBlast] Committed center=%s radius=%.0f targets=%d"),
		*Point.ToString(), TargetAreaRadius, Targets.Num());
	GetWorld()->GetTimerManager().ClearTimer(TargetTimeoutTimer);
	for (AActor* Target : Targets)
	{
		FVector PushDirection = (Target->GetActorLocation() - Point).GetSafeNormal();
		if (PushDirection.IsNearlyZero())
		{
			PushDirection = Avatar->GetActorForwardVector().GetSafeNormal2D();
		}
		ApplyCrunchDamageOnce(Target, FName(TEXT("GroundBlast")), 0.f, PushDirection * TargetPushSpeed,
			FAuraGameplayTags::Get().Damage_Physical);
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		// This cue is authority-only, not predicted locally. Clear the scoped key
		// so the multicast also executes on the predicting owner.
		FScopedPredictionWindow CuePrediction(ASC, FPredictionKey(), false);
		FGameplayCueParameters Cue;
		Cue.Location = Point;
		Cue.RawMagnitude = TargetAreaRadius;
		ASC->ExecuteGameplayCue(AuraCrunchTags::Request(TEXT("GameplayCue.Crunch.GroundBlast")), Cue);
	}
	// Keep the execution alive while GAS replicates and finishes the cast montage.
	// A bounded recovery also covers absent meshes/montages on dedicated servers.
	GetWorld()->GetTimerManager().SetTimer(RecoveryTimer, this,
		&UAuraCrunchGroundBlast::HandleCastFinished, 2.f, false);
	RegisterCrunchTimer(RecoveryTimer);
	if (MontageTask) MontageTask->EndTask();
	MontageTask = nullptr;
	if (CastMontage && CastMontage->GetPlayLength() > 0.f)
	{
		MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, CastMontage);
		if (MontageTask)
		{
			MontageTask->OnCompleted.AddDynamic(this, &UAuraCrunchGroundBlast::HandleCastFinished);
			MontageTask->OnInterrupted.AddDynamic(this, &UAuraCrunchGroundBlast::HandleCastFinished);
			MontageTask->OnCancelled.AddDynamic(this, &UAuraCrunchGroundBlast::HandleCastFinished);
			MontageTask->ReadyForActivation();
		}
	}
}
