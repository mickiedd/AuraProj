// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interaction/AuraInteractionTypes.h"
#include "AuraTargetableInterface.generated.h"

class UAuraCombatIdentityComponent;
class UAuraCombatStateComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UAuraTargetableInterface : public UInterface
{
	GENERATED_BODY()
};

class AURA_API IAuraTargetableInterface
{
	GENERATED_BODY()

public:
	virtual FGameplayTag GetAuraTargetKind() const = 0;
	virtual FText GetAuraTargetDisplayName() const = 0;
	virtual FName GetAuraTargetZoneId() const { return NAME_None; }
	virtual const UAuraCombatIdentityComponent* GetAuraTargetIdentity() const { return nullptr; }
	virtual const UAuraCombatStateComponent* GetAuraTargetState() const { return nullptr; }
	virtual float GetAuraTargetHealth() const { return 0.f; }
	virtual float GetAuraTargetMaxHealth() const { return 0.f; }
	virtual void GetAuraInteractionOptions(const AActor* RequestingActor, TArray<FAuraInteractionOption>& OutOptions) const { OutOptions.Reset(); }
	virtual bool ExecuteAuraInteraction(const AActor* RequestingActor, FGameplayTag OptionTag) const { return false; }
	virtual void SetAuraTargetHighlighted(bool bHighlighted) {}
};
