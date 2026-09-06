#pragma once
#include "Commandlets/Commandlet.h"
#include "AuraCreateTornadoMontageCommandlet.generated.h"

UCLASS()
class AURAEDITOR_API UAuraCreateTornadoMontageCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params) override;
};
