// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "UObject/Object.h"
#include "AuraWidgetControllerTestReceiver.generated.h"

/** UHT-visible receiver used to verify Blueprint-assignable widget-controller delegates. */
UCLASS(Transient, NotBlueprintable)
class UAuraWidgetControllerTestReceiver : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void ReceiveAbilityInfo(const FAuraAbilityInfo& Info)
	{
		ReceivedAbilityInfo.Add(Info);
	}

	TArray<FAuraAbilityInfo> ReceivedAbilityInfo;
};
