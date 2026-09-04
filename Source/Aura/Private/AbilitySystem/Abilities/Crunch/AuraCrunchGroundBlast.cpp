// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchGroundBlast.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "Aura/AuraLogChannels.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FGameplayTag CrunchGroundBlastTag(const TCHAR* Name)
	{
		return FGameplayTag::RequestGameplayTag(FName(Name), false);
	}
}

UAuraCrunchGroundBlast::UAuraCrunchGroundBlast()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(CrunchGroundBlastTag(TEXT("Abilities.Melee.CrunchGroundBlast")));
	SetAssetTags(AssetTags);
	StartupInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.3")), false);
	CrunchStartupInputTagName = FName(TEXT("InputTag.3"));
	CrunchCooldownTag = CrunchGroundBlastTag(TEXT("Cooldown.Melee.CrunchGroundBlast"));
	CrunchManaCost = 0.f;
	CrunchCooldown = 0.f;
	DefaultCrunchDamage = 45.f;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> TargetingFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_GroundBlast_Targetting.AM_GroundBlast_Targetting"));
	if (TargetingFinder.Succeeded()) TargetingMontage = TargetingFinder.Object;
	static ConstructorHelpers::FObjectFinder<UAnimMontage> CastFinder(
		TEXT("/Game/Assets/Characters/Crunch/Animations/Abilities/AM_GroundBlast_Casting.AM_GroundBlast_Casting"));
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
	TargetDataTask = UTargetDataUnderMouse::CreateTargetDataUnderMouse(this);
	if (!TargetDataTask)
	{
		K2_EndAbility();
		return;
	}
	TargetDataTask->ValidData.AddDynamic(this, &UAuraCrunchGroundBlast::HandleTargetData);
	TargetDataTask->ReadyForActivation();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(TargetTimeoutTimer, this, &UAuraCrunchGroundBlast::HandleTargetTimeout, 5.f, false);
		RegisterCrunchTimer(TargetTimeoutTimer);
	}
}

void UAuraCrunchGroundBlast::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (TargetDataTask) TargetDataTask->EndTask();
	if (MontageTask) MontageTask->EndTask();
	TargetDataTask = nullptr;
	MontageTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAuraCrunchGroundBlast::HandleTargetData(const FGameplayAbilityTargetDataHandle& DataHandle)
{
	if (bCommitted || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || DataHandle.Num() == 0)
	{
		return;
	}
	const FHitResult Hit = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(DataHandle, 0);
	FVector Point;
	if (!ValidateGroundTarget(Hit, Point))
	{
		UE_LOG(LogAura, Verbose, TEXT("[CrunchGroundBlast] Rejected target data: range/LOS/surface validation failed."));
		K2_EndAbility();
		return;
	}
	if (!K2_CommitAbility())
	{
		K2_EndAbility();
		return;
	}
	CommitGroundBlast(Point);
}

void UAuraCrunchGroundBlast::HandleTargetTimeout()
{
	if (!bCommitted) K2_EndAbility();
}

bool UAuraCrunchGroundBlast::ValidateGroundTarget(const FHitResult& HitResult, FVector& OutPoint) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority() || !HitResult.bBlockingHit || !FMath::IsFinite(HitResult.ImpactPoint.X)
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
		Point = Origin + FlatDelta.GetSafeNormal() * TargetTraceRange;
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
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, Params)
		|| GroundHit.ImpactNormal.Z < 0.5f)
	{
		return false;
	}
	OutPoint = GroundHit.ImpactPoint;
	return true;
}

void UAuraCrunchGroundBlast::CommitGroundBlast(const FVector& Point)
{
	if (bCommitted) return;
	bCommitted = true;
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;
	TArray<AActor*> Targets;
	CollectCrunchTargets(Point, TargetAreaRadius, Targets, TargetTraceRange);
	for (AActor* Target : Targets)
	{
		const FVector PushDirection = (Target->GetActorLocation() - Point).GetSafeNormal();
		ApplyCrunchDamageOnce(Target, FName(TEXT("GroundBlast")), 0.f, PushDirection * TargetPushSpeed,
			FAuraGameplayTags::Get().Damage_Physical);
	}
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayCueParameters Cue;
		Cue.Location = Point;
		Cue.RawMagnitude = TargetAreaRadius;
		ASC->ExecuteGameplayCue(CrunchGroundBlastTag(TEXT("GameplayCue.Crunch.GroundBlast")), Cue);
	}
	if (CastMontage)
	{
		if (UAnimInstance* AnimInstance = GetOwningComponentFromActorInfo()
			? GetOwningComponentFromActorInfo()->GetAnimInstance() : nullptr)
		{
			AnimInstance->Montage_Play(CastMontage);
		}
	}
	K2_EndAbility();
}
