// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AuraCivilianActivityMarker.generated.h"

/** Authority-registered destination shared by the Civilian Behavior Tree. */
UCLASS(Abstract)
class AURA_API AAuraCivilianActivityMarker : public AActor
{
	GENERATED_BODY()

public:
	AAuraCivilianActivityMarker();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	FName GetMarkerId() const { return MarkerId; }
	FName GetZoneId() const { return ZoneId; }
	bool IsEnabled() const { return bEnabled; }
	int32 GetCapacity() const { return Capacity; }
	float GetAcceptanceRadius() const { return AcceptanceRadius; }
	const FGameplayTagContainer& GetRequiredGameplayTags() const { return RequiredGameplayTags; }

	void SetMarkerIdForRuntime(FName InMarkerId);
	void SetZoneIdForRuntime(FName InZoneId);
	bool CanAccept(FName WorkProfileId, const FGameplayTagContainer& RequiredTags) const;

protected:
	void RegisterWithPopulationManager();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Civilian Marker")
	FName MarkerId = NAME_None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Civilian Marker")
	FName ZoneId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker", meta = (ClampMin = "1"))
	int32 Capacity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker", meta = (ClampMin = "1.0"))
	float AcceptanceRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Civilian Marker")
	FGameplayTagContainer RequiredGameplayTags;
};
