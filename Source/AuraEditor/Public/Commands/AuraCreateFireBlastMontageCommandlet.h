// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraCreateFireBlastMontageCommandlet.generated.h"

/** Creates the Aura-skeleton FireBlast cast montage from the existing fire cast animation. */
UCLASS()
class AURAEDITOR_API UAuraCreateFireBlastMontageCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
