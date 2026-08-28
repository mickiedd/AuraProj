// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Economy/AuraEconomyTypes.h"
#include "GameplayTagContainer.h"
#include "Interaction/AuraInteractionTypes.h"
#include "AuraInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAuraInteractionResultSignature, int32, ClientRequestId, EAuraInteractionResultCode, ResultCode);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraInteractionComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RequestInteraction(AActor* TargetActor, FGameplayTag InteractionOptionTag, int32 ClientRequestId);
	void RequestPurchase(AActor* MerchantActor, FName OfferId);
#if !UE_BUILD_SHIPPING
	void RequestPurchaseWithRequestIdForDevelopmentProbe(AActor* MerchantActor, FName OfferId, uint64 RequestId);
	void RequestPurchaseWithSessionNonceForDevelopmentProbe(AActor* MerchantActor, FName OfferId, const FGuid& SessionNonce, uint64 RequestId);
#endif
	void InitializeCommerceSessionNonce(const FGuid& InSessionNonce);
	const FGuid& GetCommerceSessionNonce() const { return CommerceSessionNonce; }
	uint64 GetNextCommerceRequestId() const { return NextCommerceRequestId; }

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FAuraInteractionResultSignature OnInteractionResult;

	DECLARE_MULTICAST_DELEGATE_SixParams(FAuraPurchaseResultDelegate, const FGuid& /*SessionNonce*/, uint64 /*RequestId*/, EAuraCommerceResultCode /*ResultCode*/, uint32 /*WalletRevision*/, uint32 /*InventoryRevision*/, uint32 /*StockRevision*/);
	FAuraPurchaseResultDelegate OnPurchaseResult;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestInteraction(AActor* TargetActor, FGameplayTag InteractionOptionTag, int32 ClientRequestId);

	UFUNCTION(Client, Reliable)
	void ClientInteractionResult(int32 ClientRequestId, EAuraInteractionResultCode ResultCode);

	UFUNCTION(Server, Reliable)
	void ServerRequestPurchase(FGuid SessionNonce, uint64 RequestId, AActor* MerchantActor, FName OfferId);

	UFUNCTION(Client, Reliable)
	void ClientPurchaseResult(FGuid SessionNonce, uint64 RequestId, EAuraCommerceResultCode ResultCode, uint32 WalletRevision, uint32 InventoryRevision, uint32 StockRevision);

	void ReturnResult(int32 ClientRequestId, EAuraInteractionResultCode ResultCode);
	int32 LastAcceptedRequestId = 0;
	double LastAcceptedRequestTime = -1.0;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Commerce")
	FGuid CommerceSessionNonce;
	uint64 NextCommerceRequestId = 1;
};
