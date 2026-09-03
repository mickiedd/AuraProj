// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraCreateCrunchPresentationCommandlet.generated.h"

/**
 * Materializes the validated Crunch mesh/skeleton and four source animation
 * sequences into Aura-owned packages, then authors an Aura-notify combo
 * montage. The source project remains read-only and no Crunch gameplay code is
 * loaded by the target package.
 */
UCLASS()
class AURAEDITOR_API UAuraCreateCrunchPresentationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
