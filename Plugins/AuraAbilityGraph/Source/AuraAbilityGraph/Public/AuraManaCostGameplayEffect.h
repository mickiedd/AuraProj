// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AuraManaCostGameplayEffect.generated.h"

UCLASS()
class AURAABILITYGRAPH_API UAuraManaCostGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAuraManaCostGameplayEffect()
	{
		DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo& Info = Modifiers.AddDefaulted_GetRef();
		Info.Attribute = UAuraAttributeSet::GetManaAttribute();
		Info.ModifierOp = EGameplayModOp::Additive;

		FSetByCallerFloat SetByCaller;
		SetByCaller.DataName = FName("Abilities.Cost.Mana");
		Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}
};