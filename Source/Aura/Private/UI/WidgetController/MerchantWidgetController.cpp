// Copyright Druid Mechanics

#include "UI/WidgetController/MerchantWidgetController.h"

#include "Character/AuraCivilian.h"
#include "Economy/AuraMerchantComponent.h"
#include "Interaction/AuraInteractionComponent.h"
#include "Player/AuraPlayerController.h"

void UMerchantWidgetController::BeginDestroy()
{
	if (MerchantComponent)
	{
		MerchantComponent->OnPresentationChanged.RemoveAll(this);
		if (AAuraPlayerController* Controller = GetAuraPC())
		{
			if (Controller->GetInteractionComponent()) Controller->GetInteractionComponent()->OnPurchaseResult.RemoveAll(this);
		}
	}
	Super::BeginDestroy();
}

void UMerchantWidgetController::SetMerchantComponent(UAuraMerchantComponent* InMerchantComponent)
{
	if (MerchantComponent == InMerchantComponent) return;
	if (MerchantComponent) MerchantComponent->OnPresentationChanged.RemoveAll(this);
	if (AAuraPlayerController* Controller = GetAuraPC())
	{
		if (Controller->GetInteractionComponent()) Controller->GetInteractionComponent()->OnPurchaseResult.RemoveAll(this);
	}
	MerchantComponent = InMerchantComponent;
	MerchantPresentation = MerchantComponent ? MerchantComponent->GetPresentation() : FAuraMerchantPresentation();
	if (MerchantComponent)
	{
		MerchantComponent->OnPresentationChanged.AddUObject(this, &UMerchantWidgetController::HandleMerchantPresentation);
		OnMerchantPresentation.Broadcast(MerchantPresentation);
	}
	if (AAuraPlayerController* Controller = GetAuraPC())
	{
		if (Controller->GetInteractionComponent()) Controller->GetInteractionComponent()->OnPurchaseResult.AddUObject(this, &UMerchantWidgetController::HandlePurchaseResult);
	}
}

void UMerchantWidgetController::PurchaseOffer(FName OfferId)
{
	AAuraPlayerController* Controller = GetAuraPC();
	if (!Controller || !Controller->GetInteractionComponent() || !IsMerchantOpen() || OfferId.IsNone()) return;
	Controller->GetInteractionComponent()->RequestPurchase(MerchantComponent->GetOwner(), OfferId);
}

bool UMerchantWidgetController::IsMerchantOpen() const
{
	if (!IsValid(MerchantComponent) || !MerchantComponent->IsMerchantActive()) return false;
	const AAuraCivilian* Merchant = Cast<AAuraCivilian>(MerchantComponent->GetOwner());
	const AAuraPlayerController* Controller = Cast<AAuraPlayerController>(PlayerController);
	const APawn* Requester = Controller ? Controller->GetPawn() : nullptr;
	return Merchant && Merchant->IsCombatAlive() && Requester
		&& FVector::DistSquared(Requester->GetActorLocation(), Merchant->GetActorLocation()) <= FMath::Square(250.f);
}

void UMerchantWidgetController::HandleMerchantPresentation(const FAuraMerchantPresentation& InPresentation)
{
	MerchantPresentation = InPresentation;
	OnMerchantPresentation.Broadcast(MerchantPresentation);
}

void UMerchantWidgetController::HandlePurchaseResult(const FGuid& SessionNonce, uint64 RequestId, EAuraCommerceResultCode ResultCode,
	uint32 WalletRevision, uint32 InventoryRevision, uint32 StockRevision)
{
	OnPurchaseResult.Broadcast(SessionNonce, RequestId, ResultCode, WalletRevision, InventoryRevision, StockRevision);
}
