// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/AuraCombatTypes.h"
#include "Interaction/AuraInteractionTypes.h"
#include "AuraTargetingTypes.generated.h"

USTRUCT(BlueprintType)
struct AURA_API FAuraTargetDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Target") FGameplayTag RelationshipTag;
	UPROPERTY(BlueprintReadOnly, Category = "Target") FGameplayTag KindTag;
	UPROPERTY(BlueprintReadOnly, Category = "Target") FGameplayTag LifeTag;
	UPROPERTY(BlueprintReadOnly, Category = "Target") TArray<FAuraInteractionOption> InteractionOptions;
	UPROPERTY(BlueprintReadOnly, Category = "Target") bool bLocallyAttackAllowed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Target") EAuraCombatRuleRejectionReason AttackRejectionReason = EAuraCombatRuleRejectionReason::InvalidTarget;
	UPROPERTY(BlueprintReadOnly, Category = "Target") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Target") float MaxHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Target") FText DisplayName;

	bool IsValid() const { return KindTag.IsValid() && LifeTag.IsValid(); }
};
