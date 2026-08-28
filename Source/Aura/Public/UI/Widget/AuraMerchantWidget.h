// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/AuraUserWidget.h"
#include "AuraMerchantWidget.generated.h"

class UMerchantWidgetController;

/** Minimal Blueprint-ready merchant panel surface; row visuals are driven by the controller snapshot. */
UCLASS()
class AURA_API UAuraMerchantWidget : public UAuraUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Merchant")
	void SetMerchantWidgetController(UMerchantWidgetController* InController);

	UFUNCTION(BlueprintCallable, Category = "Merchant")
	void BuyOffer(FName OfferId);

	UFUNCTION(BlueprintPure, Category = "Merchant")
	bool IsMerchantOpen() const;

private:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Merchant", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMerchantWidgetController> MerchantController;
};
