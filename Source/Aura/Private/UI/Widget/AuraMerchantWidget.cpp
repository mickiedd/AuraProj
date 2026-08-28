// Copyright Druid Mechanics

#include "UI/Widget/AuraMerchantWidget.h"

#include "UI/WidgetController/MerchantWidgetController.h"

void UAuraMerchantWidget::SetMerchantWidgetController(UMerchantWidgetController* InController)
{
	MerchantController = InController;
	SetWidgetController(InController);
}

void UAuraMerchantWidget::BuyOffer(FName OfferId)
{
	if (MerchantController) MerchantController->PurchaseOffer(OfferId);
}

bool UAuraMerchantWidget::IsMerchantOpen() const
{
	return MerchantController && MerchantController->IsMerchantOpen();
}
