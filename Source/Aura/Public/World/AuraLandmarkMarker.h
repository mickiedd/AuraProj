// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuraLandmarkMarker.generated.h"

/**
 * Authoring marker for a destination that can be reached by the landmark guide.
 * The actor's transform is the point the player should look at; ApproachTransform
 * is deliberately separate so a large building's collision is never used as a
 * walk destination.
 */
UCLASS(Blueprintable, BlueprintType)
class AURA_API AAuraLandmarkMarker : public AActor
{
	GENERATED_BODY()

public:
	AAuraLandmarkMarker();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Landmark")
	FName LandmarkId;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Landmark")
	FText DisplayName;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Landmark")
	int32 SortOrder = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Landmark")
	bool bEnabled = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Landmark|Guide")
	FTransform ApproachTransform;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Landmark|Guide", meta=(ClampMin="25.0", UIMin="25.0"))
	float ArrivalRadius = 150.f;

	UFUNCTION(BlueprintPure, Category="Landmark")
	FTransform GetWorldApproachTransform() const;

	UFUNCTION(BlueprintPure, Category="Landmark")
	bool IsGuideConfigured() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
