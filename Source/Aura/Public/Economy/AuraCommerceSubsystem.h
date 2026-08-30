// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Economy/AuraEconomyTypes.h"
#include "Game/AuraSaveTypes.h"
#include "Interaction/AuraInteractionTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "AuraCommerceSubsystem.generated.h"

class AAuraCivilian;
class AAuraPlayerController;
class UAuraMerchantComponent;

struct AURA_API FAuraPurchaseResult
{
	FGuid SessionNonce;
	uint64 RequestId = 0;
	EAuraCommerceResultCode ResultCode = EAuraCommerceResultCode::InvalidRequest;
	uint32 WalletRevision = 0;
	uint32 InventoryRevision = 0;
	uint32 StockRevision = 0;
	bool bReplay = false;
};

/** Authority-only owner of merchant stock, request sequencing, replay cache, and atomic buys. */
UCLASS(Transient)
class AURA_API UAuraCommerceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Creates a fresh nonce and clears the old connection's replay window. */
	FGuid RotateSession(AAuraPlayerController* PlayerController);
	void InvalidateSession(AAuraPlayerController* PlayerController);

	/** Registers or rebinds one canonical population-member merchant. */
	bool RegisterMerchant(UAuraMerchantComponent* MerchantComponent);
	void UnregisterMerchant(UAuraMerchantComponent* MerchantComponent);
	/** Marks a still-bound merchant unavailable while its population slot is dormant. */
	void MarkMerchantUnavailable(UAuraMerchantComponent* MerchantComponent);

	/** Processes one authenticated owned-endpoint request on the authority game thread. */
	bool ProcessPurchase(AAuraPlayerController* PlayerController, const FGuid& SessionNonce, uint64 RequestId,
		AActor* MerchantActor, FName OfferId, FAuraPurchaseResult& OutResult);

	int32 GetRegisteredMerchantCount() const { return Merchants.Num(); }
	bool GetOfferStock(FName PopulationMemberId, FName OfferId, int64& OutStock, uint32& OutRevision) const;
	void CapturePersistenceState(TArray<FAuraPersistedMerchantStock>& OutStocks) const;
	bool RestorePersistenceState(const TArray<FAuraPersistedMerchantStock>& Stocks, FString& OutError);
	/** Applies one server-UTC restock evaluation using a monotonic observed clock. */
	bool EvaluateRestock(FName PopulationMemberId, const FDateTime& ObservedUtc, bool bStartup, FName& OutResultCode);
#if !UE_BUILD_SHIPPING
	/** Development-only fixture control; never compiled into Shipping. */
	bool SetOfferStockForDevelopmentProbe(FName PopulationMemberId, FName OfferId, int64 NewStock);
	bool SetMerchantAvailabilityForDevelopmentProbe(FName PopulationMemberId, bool bInAvailable);
	bool EvaluateRestockForDevelopmentProbe(FName PopulationMemberId, const FDateTime& ObservedUtc, bool bStartup);
#endif

private:
	struct FRuntimeOffer
	{
		FAuraOfferDefinition Definition;
		int64 CurrentStock = 0;
	};

	struct FRuntimeMerchant
	{
		FName PopulationMemberId = NAME_None;
		FName MerchantDefinitionId = NAME_None;
		TWeakObjectPtr<UAuraMerchantComponent> Component;
		TMap<FName, FRuntimeOffer> Offers;
		uint32 StockRevision = 1;
		bool bAvailable = false;
		FDateTime LastRestockAtUtc;
		FDateTime LastObservedAtUtc;
		FTimerHandle RestockTimerHandle;
	};

	struct FSession
	{
		FGuid Nonce;
		uint64 LastAcceptedRequestId = 0;
		TMap<uint64, FAuraPurchaseResult> ReplayCache;
		TArray<uint64> ReplayOrder;
	};

	FAuraPurchaseResult ExecutePurchase(AAuraPlayerController* PlayerController, FRuntimeMerchant& Merchant,
		AActor* MerchantActor, FName OfferId);
	void CacheResult(FSession& Session, const FAuraPurchaseResult& Result);
	void FillRevisions(AAuraPlayerController* PlayerController, const FRuntimeMerchant* Merchant, FName OfferId,
		FAuraPurchaseResult& Result) const;
	void PublishMerchantPresentation(FRuntimeMerchant& Merchant);
	static EAuraCommerceResultCode MapInteractionFailure(EAuraInteractionResultCode Failure);

	TMap<FName, FRuntimeMerchant> Merchants;
	TMap<FName, FAuraPersistedMerchantStock> PendingMerchantRestores;
	TMap<TWeakObjectPtr<AAuraPlayerController>, FSession> Sessions;
};
