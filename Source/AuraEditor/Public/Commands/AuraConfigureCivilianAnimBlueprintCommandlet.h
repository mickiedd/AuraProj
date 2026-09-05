// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraConfigureCivilianAnimBlueprintCommandlet.generated.h"

/** Enables the inherited movement update path used by Civilian's Shaman AnimBP. */
UCLASS()
class AURAEDITOR_API UAuraConfigureCivilianAnimBlueprintCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
