// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AuraLandmarkWorldSubsystem.generated.h"

class AAuraLandmarkMarker;

USTRUCT(BlueprintType)
struct FAuraLandmarkDescriptor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName LandmarkId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FVector LookAtLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FTransform ApproachTransform;

	UPROPERTY(BlueprintReadOnly)
	float ArrivalRadius = 150.f;

	UPROPERTY(BlueprintReadOnly)
	int32 SortOrder = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly)
	FString UnavailableReason;

	TWeakObjectPtr<AAuraLandmarkMarker> Marker;
};

/** World-owned source of truth for the current map's guide destinations. */
UCLASS()
class AURA_API UAuraLandmarkWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterMarker(AAuraLandmarkMarker* Marker);
	void UnregisterMarker(AAuraLandmarkMarker* Marker);

	UFUNCTION(BlueprintCallable, Category="Landmark")
	void GetLandmarkCatalog(TArray<FAuraLandmarkDescriptor>& OutCatalog) const;

	UFUNCTION(BlueprintCallable, Category="Landmark")
	bool ResolveLandmark(FName LandmarkId, FAuraLandmarkDescriptor& OutDescriptor) const;

	UFUNCTION(BlueprintCallable, Category="Landmark")
	int32 GetCatalogRevision() const { return CatalogRevision; }

private:
	void RebuildCatalog() const;

	UPROPERTY()
	TMap<FName, TObjectPtr<AAuraLandmarkMarker>> RegisteredMarkers;

	mutable TArray<FAuraLandmarkDescriptor> CachedCatalog;
	mutable bool bCatalogDirty = true;
	int32 CatalogRevision = 1;
};
