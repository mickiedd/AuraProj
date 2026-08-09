// Copyright Druid Mechanics


#include "AbilitySystem/Abilities/AuraDamageGameplayAbility.h"

#include "AbilitySystem/AuraAbilitySystemComponent.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AuraGameplayTags.h"
#include "Aura/AuraLogChannels.h"

void UAuraDamageGameplayAbility::CauseDamage(AActor* TargetActor)
{
	FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(TargetActor);
	UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
}

FGameplayTag UAuraDamageGameplayAbility::GetDamageAbilityTag() const
{
	if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		const UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
		if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(CurrentSpecHandle))
		{
			if (const FGameplayTag AbilityTag = UAuraAbilitySystemComponent::GetAbilityTagFromSpec(*Spec); AbilityTag.IsValid())
			{
				return AbilityTag;
			}
		}
	}

	const FGameplayTag AbilitiesRoot = FGameplayTag::RequestGameplayTag(FName(TEXT("Abilities")), false);
	for (const FGameplayTag& Tag : AbilityTags)
	{
		if (Tag.IsValid() && (!AbilitiesRoot.IsValid() || Tag.MatchesTag(AbilitiesRoot)))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}

FDamageEffectParams UAuraDamageGameplayAbility::MakeDamageEffectParamsFromClassDefaults(AActor* TargetActor,
	FVector InRadialDamageOrigin, bool bOverrideKnockbackDirection, FVector KnockbackDirectionOverride,
	bool bOverrideDeathImpulse, FVector DeathImpulseDirectionOverride, bool bOverridePitch, float PitchOverride) const
{
	AActor* SourceAvatarActor = CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid()
		? CurrentActorInfo->AvatarActor.Get()
		: nullptr;
	UAbilitySystemComponent* SourceASC = CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid()
		? CurrentActorInfo->AbilitySystemComponent.Get()
		: nullptr;
	const int32 AbilityLevel = CurrentActorInfo ? GetAbilityLevel() : 1;

	FDamageEffectParams Params;
	Params.WorldContextObject = SourceAvatarActor;
	Params.DamageGameplayEffectClass = DamageEffectClass;
	Params.SourceAbilitySystemComponent = SourceASC;
	Params.TargetAbilitySystemComponent = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	Params.BaseDamage = Damage.GetValueAtLevel(AbilityLevel);
	Params.AbilityLevel = AbilityLevel;
	Params.DamageType = DamageType;
	Params.AbilityTag = GetDamageAbilityTag();
	Params.CombatRuleContext.QueryPurpose = EAuraCombatQueryPurpose::Damage;
	Params.CombatRuleContext.TrustedWorldContext = Params.WorldContextObject;
	Params.CombatRuleContext.SourceActor = SourceAvatarActor;
	Params.CombatRuleContext.TargetActor = TargetActor;
	if (TargetActor)
	{
		Params.CombatRuleContext.ImpactLocation = TargetActor->GetActorLocation();
	}
	Params.DebuffChance = DebuffChance;
	Params.DebuffDamage = DebuffDamage;
	Params.DebuffDuration = DebuffDuration;
	Params.DebuffFrequency = DebuffFrequency;
	Params.DeathImpulseMagnitude = DeathImpulseMagnitude;
	Params.KnockbackForceMagnitude = KnockbackForceMagnitude;
	Params.KnockbackChance = KnockbackChance;

	if (IsValid(TargetActor) && IsValid(SourceAvatarActor))
	{
		FRotator Rotation = (TargetActor->GetActorLocation() - SourceAvatarActor->GetActorLocation()).Rotation();
		if (bOverridePitch)
		{
			Rotation.Pitch = PitchOverride;
		}
		const FVector ToTarget = Rotation.Vector();
		if (!bOverrideKnockbackDirection)
		{
			Params.KnockbackForce = ToTarget * KnockbackForceMagnitude;
		}
		if (!bOverrideDeathImpulse)
		{
			Params.DeathImpulse = ToTarget * DeathImpulseMagnitude;
		}
	}
	
	
	if (bOverrideKnockbackDirection)
	{
		KnockbackDirectionOverride.Normalize();
		Params.KnockbackForce = KnockbackDirectionOverride * KnockbackForceMagnitude;
		if (bOverridePitch)
		{
			FRotator KnockbackRotation = KnockbackDirectionOverride.Rotation();
			KnockbackRotation.Pitch = PitchOverride;
			Params.KnockbackForce = KnockbackRotation.Vector() * KnockbackForceMagnitude;
		}
	}

	if (bOverrideDeathImpulse)
	{
		DeathImpulseDirectionOverride.Normalize();
		Params.DeathImpulse = DeathImpulseDirectionOverride * DeathImpulseMagnitude;
		if (bOverridePitch)
		{
			FRotator DeathImpulseRotation = DeathImpulseDirectionOverride.Rotation();
			DeathImpulseRotation.Pitch = PitchOverride;
			Params.DeathImpulse = DeathImpulseRotation.Vector() * DeathImpulseMagnitude;
		}
	}
	
	if (bIsRadialDamage)
	{
		Params.bIsRadialDamage = bIsRadialDamage;
		Params.RadialDamageOrigin = InRadialDamageOrigin;
		Params.RadialDamageInnerRadius = RadialDamageInnerRadius;
		Params.RadialDamageOuterRadius = RadialDamageOuterRadius;
	}
	return Params;
}

float UAuraDamageGameplayAbility::GetDamageAtLevel() const
{
	return Damage.GetValueAtLevel(GetAbilityLevel());
}

FTaggedMontage UAuraDamageGameplayAbility::GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const
{
	if (TaggedMontages.Num() > 0)
	{
		const int32 Selection = FMath::RandRange(0, TaggedMontages.Num() - 1);
		const bool bIsServer = CurrentActorInfo && CurrentActorInfo->IsNetAuthority();
		UE_LOG(LogAura, Warning, TEXT("[MontageSelect] Ability=%s IsNetAuth=%d Selection=%d Count=%d Tag=%s"),
			*GetNameSafe(this),
			bIsServer,
			Selection,
			TaggedMontages.Num(),
			*TaggedMontages[Selection].MontageTag.ToString());
		return TaggedMontages[Selection];
	}

	return FTaggedMontage();
}
