// Copyright Druid Mechanics

#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraEnsureBungeeMontageSlotCommandlet.generated.h"

/** Ensures BungeeMan's AnimGraph evaluates the DefaultSlot montage over locomotion. */
UCLASS()
class AURAEDITOR_API UAuraEnsureBungeeMontageSlotCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
