// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AuraCooldownGameplayEffect.generated.h"

UCLASS()
class AURAABILITYGRAPH_API UAuraCooldownGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UAuraCooldownGameplayEffect()
	{
		DurationPolicy = EGameplayEffectDurationType::HasDuration;
		DurationMagnitude = FScalableFloat(1.0f);
		Period = 0.0f;
	}
};
