// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Combat/AuraCombatTypes.h"
#include "Battle/AuraBattleZoneTypes.h"
#include "AuraInteractionTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraInteractionResultCode : uint8
{
	Success,
	InvalidRequester,
	InvalidTarget,
	OptionUnavailable,
	WrongTargetKind,
	WrongLifeState,
	PhaseDenied,
	ZoneDenied,
	OutOfRange,
	LineOfSightBlocked,
	RateLimited,
	StaleRequest,
	FeatureUnavailable,
};

USTRUCT(BlueprintType)
struct AURA_API FAuraInteractionOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction") FGameplayTag OptionTag;
	UPROPERTY(BlueprintReadOnly, Category = "Interaction") FText DisplayText;
	UPROPERTY(BlueprintReadOnly, Category = "Interaction") bool bEnabled = false;
	UPROPERTY(BlueprintReadOnly, Category = "Interaction") EAuraInteractionResultCode DisabledReason = EAuraInteractionResultCode::OptionUnavailable;
};

USTRUCT()
struct AURA_API FAuraResolvedInteractionPolicy
{
	GENERATED_BODY()

	FGameplayTag OptionTag;
	FName HandlerId = NAME_None;
	float MaxRangeCm = 0.f;
	bool bRequiresLineOfSight = true;
	TArray<FGameplayTag> AllowedTargetKinds;
	TArray<EAuraCombatLifeState> AllowedLifeStates;
	TArray<EAuraBattlePhase> AllowedPhases;
};
