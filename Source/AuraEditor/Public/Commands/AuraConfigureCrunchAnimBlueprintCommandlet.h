// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraConfigureCrunchAnimBlueprintCommandlet.generated.h"

/** Builds the Aura-compatible Crunch idle/jog base pose and montage slot graph. */
UCLASS()
class AURAEDITOR_API UAuraConfigureCrunchAnimBlueprintCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
