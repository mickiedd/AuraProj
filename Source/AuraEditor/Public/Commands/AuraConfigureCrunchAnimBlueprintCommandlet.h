// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraConfigureCrunchAnimBlueprintCommandlet.generated.h"

/** Adds the minimal Aura-compatible montage slot to the translated Crunch AnimBP. */
UCLASS()
class AURAEDITOR_API UAuraConfigureCrunchAnimBlueprintCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
