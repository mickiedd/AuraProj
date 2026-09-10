// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "AuraLandmarkButton.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAuraLandmarkButtonClicked, FName, LandmarkId);

UCLASS()
class AURA_API UAuraLandmarkButton : public UButton
{
	GENERATED_BODY()

public:
	UAuraLandmarkButton();

	UPROPERTY(BlueprintAssignable, Category="Landmark")
	FAuraLandmarkButtonClicked OnLandmarkClicked;

	void SetLandmarkId(FName InLandmarkId) { LandmarkId = InLandmarkId; }

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	FName LandmarkId;
};
