// Copyright Druid Mechanics

#include "AbilitySystem/Abilities/Crunch/AuraCrunchAbilityBase.h"

#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraGameplayTags.h"
#include "Combat/AuraCombatRules.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/CombatInterface.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

#include "AuraAbilityGraph/Public/AuraCooldownGameplayEffect.h"
#include "AuraAbilityGraph/Public/AuraManaCostGameplayEffect.h"

UAuraCrunchAbilityBase::UAuraCrunchAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	CostGameplayEffectClass = UAuraManaCostGameplayEffect::StaticClass();
	CooldownGameplayEffectClass = UAuraCooldownGameplayEffect::StaticClass();
	DefaultCrunchDamageType = FAuraGameplayTags::Get().Damage_Physical;
}

FGameplayTag UAuraCrunchAbilityBase::GetStartupInputTag() const
{
	if (StartupInputTag.IsValid())
	{
		return StartupInputTag;
	}
	return CrunchStartupInputTagName.IsNone()
		? FGameplayTag()
		: FGameplayTag::RequestGameplayTag(CrunchStartupInputTagName, false);
}

bool UAuraCrunchAbilityBase::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || ActorInfo->IsNetAuthority() == false)
	{
		return true;
	}
	if (CrunchManaCost <= 0.f)
	{
		return true;
	}
	bool bFoundMana = false;
	const float Mana = ActorInfo->AbilitySystemComponent->GetGameplayAttributeValue(UAuraAttributeSet::GetManaAttribute(), bFoundMana);
	return !bFoundMana || Mana >= CrunchManaCost;
}

void UAuraCrunchAbilityBase::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || CrunchManaCost <= 0.f)
	{
		return;
	}
	const FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
		UAuraManaCostGameplayEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}
	UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(SpecHandle, FName("Abilities.Cost.Mana"), -CrunchManaCost);
	ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UAuraCrunchAbilityBase::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || CrunchCooldown <= 0.f)
	{
		return;
	}
	const FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
		UAuraCooldownGameplayEffect::StaticClass(), GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return;
	}
	SpecHandle.Data->SetDuration(CrunchCooldown, false);
	if (CrunchCooldownTag.IsValid())
	{
		SpecHandle.Data->DynamicGrantedTags.AddTag(CrunchCooldownTag);
	}
	ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

const FGameplayTagContainer* UAuraCrunchAbilityBase::GetCooldownTags() const
{
	CachedCrunchCooldownTags.Reset();
	if (CrunchCooldownTag.IsValid())
	{
		CachedCrunchCooldownTags.AddTag(CrunchCooldownTag);
	}
	return &CachedCrunchCooldownTags;
}

bool UAuraCrunchAbilityBase::BeginCrunchActivation(const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->AbilitySystemComponent.IsValid()
		|| !HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		return false;
	}
	CrunchHitLedger.Reset();
	CrunchTimers.Reset();
	bCrunchActivationActive = true;
	return true;
}

void UAuraCrunchAbilityBase::RegisterCrunchTimer(const FTimerHandle& Handle)
{
	CrunchTimers.Add(Handle);
}

void UAuraCrunchAbilityBase::CleanupCrunchActivation()
{
	if (UWorld* World = GetWorld())
	{
		for (const FTimerHandle& RegisteredHandle : CrunchTimers)
		{
			FTimerHandle Handle = RegisteredHandle;
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	CrunchTimers.Reset();
	CrunchHitLedger.Reset();
	bCrunchActivationActive = false;
}

bool UAuraCrunchAbilityBase::ValidateCrunchTarget(AActor* Target, float MaxRange, FString* OutReason) const
{
	AActor* Source = GetAvatarActorFromActorInfo();
	if (!IsValid(Source) || !IsValid(Target) || Source == Target)
	{
		if (OutReason) *OutReason = TEXT("InvalidSourceOrSelfTarget");
		return false;
	}
	if (MaxRange > 0.f && FVector::DistSquared(Source->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxRange))
	{
		if (OutReason) *OutReason = TEXT("OutOfRange");
		return false;
	}
	if (!Target->Implements<UCombatInterface>() || ICombatInterface::Execute_IsDead(Target))
	{
		if (OutReason) *OutReason = TEXT("DeadOrNonCombatTarget");
		return false;
	}
	FAuraCombatRuleContext Context;
	Context.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	Context.TrustedWorldContext = Source;
	Context.SourceActor = Source;
	Context.TargetActor = Target;
	const FAuraCombatRuleResult Result = FAuraCombatRules::CanDamage(Source, Target, Context);
	if (!Result.bCanDamage)
	{
		if (OutReason) *OutReason = StaticEnum<EAuraCombatRuleRejectionReason>()->GetValueAsString(Result.RejectionReason);
		return false;
	}
	return true;
}

void UAuraCrunchAbilityBase::CollectCrunchTargets(const FVector& Origin, float Radius, TArray<AActor*>& OutTargets,
	float MaxRange) const
{
	OutTargets.Reset();
	AActor* Source = GetAvatarActorFromActorInfo();
	if (!IsValid(Source) || !Source->HasAuthority() || !GetWorld() || Radius <= 0.f)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrunchTargetQuery), false);
	Params.AddIgnoredActor(Source);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects),
		FCollisionShape::MakeSphere(Radius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (ValidateCrunchTarget(Target, MaxRange > 0.f ? MaxRange : Radius))
		{
			OutTargets.AddUnique(Target);
		}
	}
}

bool UAuraCrunchAbilityBase::ApplyCrunchDamageOnce(AActor* Target, FName EventKey, float DamageOverride,
	const FVector& KnockbackVelocity, const FGameplayTag& DamageTypeOverride)
{
	AActor* Source = GetAvatarActorFromActorInfo();
	if (!IsCrunchActivationActive() || !Source || !Source->HasAuthority() || !Target)
	{
		return false;
	}
	FString Rejection;
	if (!ValidateCrunchTarget(Target, 0.f, &Rejection))
	{
		return false;
	}
	const FString LedgerKey = FString::Printf(TEXT("%s:%s"), *EventKey.ToString(), *GetNameSafe(Target));
	if (CrunchHitLedger.Contains(LedgerKey))
	{
		return false;
	}
	CrunchHitLedger.Add(LedgerKey);
	FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
	Params.BaseDamage = DamageOverride > 0.f ? DamageOverride : (Params.BaseDamage > 0.f ? Params.BaseDamage : DefaultCrunchDamage);
	Params.DamageType = DamageTypeOverride.IsValid() ? DamageTypeOverride : DefaultCrunchDamageType;
	if (!Params.DamageType.IsValid()) Params.DamageType = FAuraGameplayTags::Get().Damage_Physical;
	if (!KnockbackVelocity.IsNearlyZero())
	{
		Params.KnockbackForce = KnockbackVelocity;
		Params.KnockbackForceMagnitude = KnockbackVelocity.Size();
		// Crunch impact abilities author a concrete push velocity rather than a
		// probabilistic knockback. Keep the effect context explicit so the resolved
		// force is actually consumed by AuraAttributeSet on the server.
		Params.KnockbackChance = 100.f;
	}
	return UAuraAbilitySystemLibrary::ApplyDamageEffect(Params).IsValid();
}

void UAuraCrunchAbilityBase::LaunchCrunchCharacter(AActor* Target, const FVector& LaunchVelocity) const
{
	if (!Target || !Target->HasAuthority()) return;
	if (ACharacter* Character = Cast<ACharacter>(Target))
	{
		Character->LaunchCharacter(LaunchVelocity, true, true);
	}
}

void UAuraCrunchAbilityBase::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CleanupCrunchActivation();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
