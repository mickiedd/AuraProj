// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraValidateComboMontageCommandlet.generated.h"

/** Validates the generated Crunch combo montage contract without starting PIE. */
UCLASS()
class AURAEDITOR_API UAuraValidateComboMontageCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
