#pragma once

#include "Commandlets/Commandlet.h"
#include "AuraCreateGroundBlastMontageCommandlet.generated.h"

/** Creates the GroundBlast cast from the installed, retargeted Crunch animation. */
UCLASS()
class AURAEDITOR_API UAuraCreateGroundBlastMontageCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
