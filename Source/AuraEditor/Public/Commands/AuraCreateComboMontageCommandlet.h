// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraCreateComboMontageCommandlet.generated.h"

/**
 * Builds a small Aura-skeleton combo montage from an existing Aura montage.
 * This is an editor-only migration fixture; it keeps Crunch behavior testable
 * without importing Crunch's 1.4 GB Paragon asset tree.
 */
UCLASS()
class AURAEDITOR_API UAuraCreateComboMontageCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
