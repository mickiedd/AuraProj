// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "World/AuraCivilianActivityMarker.h"
#include "Combat/AuraTargetableInterface.h"
#include "AuraCivilianShelterMarker.generated.h"

UCLASS()
class AURA_API AAuraCivilianShelterMarker : public AAuraCivilianActivityMarker, public IAuraTargetableInterface
{
	GENERATED_BODY()

public:
	float GetProtectionRadius() const { return ProtectionRadius; }
	virtual FGameplayTag GetAuraTargetKind() const override;
	virtual FText GetAuraTargetDisplayName() const override;
	virtual FName GetAuraTargetZoneId() const override { return GetZoneId(); }
	virtual void GetAuraInteractionOptions(const AActor* RequestingActor, TArray<FAuraInteractionOption>& OutOptions) const override;
	virtual bool ExecuteAuraInteraction(const AActor* RequestingActor, FGameplayTag OptionTag) const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker|Shelter", meta = (ClampMin = "0.0"))
	float ProtectionRadius = 500.f;
};
