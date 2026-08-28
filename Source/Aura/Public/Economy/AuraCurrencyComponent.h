// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Economy/AuraEconomyTypes.h"
#include "AuraCurrencyComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FAuraCurrencyChangedDelegate, int64 /*Balance*/, uint32 /*Revision*/);

UCLASS(ClassGroup = (Economy), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraCurrencyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraCurrencyComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FName GetCurrencyId() const { return CurrencyId; }
	int64 GetBalance() const { return Balance; }
	uint32 GetRevision() const { return Revision; }

	bool CanCreditCurrency(FName InCurrencyId, int64 Amount) const;
	bool CanDebitCurrency(FName InCurrencyId, int64 Amount) const;
	EAuraEconomyMutationResult CommitCreditCurrency(FName InCurrencyId, int64 Amount, bool bBroadcastEvents = true);
	EAuraEconomyMutationResult CommitDebitCurrency(FName InCurrencyId, int64 Amount, bool bBroadcastEvents = true);

	/** Authority-only one-time bootstrap write; persistence uses RestoreCurrencyState. */
	bool InitializeCurrency(FName InCurrencyId, int64 InitialBalance);

	/** Authority-only replacement used by a rollback-safe commerce/persistence owner. */
	bool RestoreCurrencyState(FName InCurrencyId, int64 InBalance, uint32 InRevision, bool bBroadcastEvents = true);

	/** Publishes a previously silent authority mutation after an atomic transaction commits. */
	void PublishChanged() { BroadcastChanged(); }

	FAuraCurrencyChangedDelegate OnCurrencyChanged;

private:
	bool IsValidCurrency(FName InCurrencyId) const;
	bool CanUseAmount(int64 Amount) const;
	void BroadcastChanged();

	UPROPERTY(ReplicatedUsing = OnRep_CurrencyId, VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FName CurrencyId = NAME_None;

	UPROPERTY(ReplicatedUsing = OnRep_Balance, VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	int64 Balance = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Revision, VisibleInstanceOnly, meta = (AllowPrivateAccess = "true"))
	uint32 Revision = 0;

	UFUNCTION()
	void OnRep_CurrencyId();

	UFUNCTION()
	void OnRep_Balance();

	UFUNCTION()
	void OnRep_Revision();
};
