// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Economy/AuraEconomyTypes.h"
#include "UI/WidgetController/AuraWidgetController.h"
#include "MerchantWidgetController.generated.h"

class UAuraMerchantComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FAuraMerchantPresentationDelegate, const FAuraMerchantPresentation& /*Presentation*/);
DECLARE_MULTICAST_DELEGATE_SixParams(FAuraMerchantPurchaseDelegate, const FGuid& /*SessionNonce*/, uint64 /*RequestId*/, EAuraCommerceResultCode /*ResultCode*/, uint32 /*WalletRevision*/, uint32 /*InventoryRevision*/, uint32 /*StockRevision*/);

/** Controller for a basic merchant panel; all values come from replicated presentation/result events. */
UCLASS(BlueprintType, Blueprintable)
class AURA_API UMerchantWidgetController : public UAuraWidgetController
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable, Category = "Merchant")
	void SetMerchantComponent(UAuraMerchantComponent* InMerchantComponent);

	UFUNCTION(BlueprintCallable, Category = "Merchant")
	void PurchaseOffer(FName OfferId);

	UFUNCTION(BlueprintPure, Category = "Merchant")
	bool IsMerchantOpen() const;

	UFUNCTION(BlueprintPure, Category = "Merchant")
	const FAuraMerchantPresentation& GetMerchantPresentation() const { return MerchantPresentation; }

	FAuraMerchantPresentationDelegate OnMerchantPresentation;
	FAuraMerchantPurchaseDelegate OnPurchaseResult;

private:
	void HandleMerchantPresentation(const FAuraMerchantPresentation& InPresentation);
	void HandlePurchaseResult(const FGuid& SessionNonce, uint64 RequestId, EAuraCommerceResultCode ResultCode,
		uint32 WalletRevision, uint32 InventoryRevision, uint32 StockRevision);

	UPROPERTY(Transient)
	TObjectPtr<UAuraMerchantComponent> MerchantComponent;

	FAuraMerchantPresentation MerchantPresentation;
};
