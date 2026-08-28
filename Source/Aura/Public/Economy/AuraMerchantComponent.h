// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Economy/AuraEconomyTypes.h"
#include "AuraMerchantComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FAuraMerchantPresentationChangedDelegate, const FAuraMerchantPresentation& /*Presentation*/);

class UAuraCommerceSubsystem;

/** Public replicated merchant presentation backed by one stable population member. */
UCLASS(ClassGroup = (Economy), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraMerchantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraMerchantComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Authority-only binding copied from the exact population member override. */
	void InitializeAuthorityBinding(FName InPopulationMemberId, FName InMerchantDefinitionId);
	/** Authority-only immediate close used when the owning Civilian enters a non-live state. */
	void SetAuthorityUnavailable();
	const FAuraMerchantPresentation& GetPresentation() const { return Presentation; }
	FName GetPopulationMemberId() const { return Presentation.PopulationMemberId; }
	FName GetMerchantDefinitionId() const { return Presentation.MerchantDefinitionId; }
	bool IsMerchantActive() const { return Presentation.bAvailable && !Presentation.Offers.IsEmpty(); }

	FAuraMerchantPresentationChangedDelegate OnPresentationChanged;

private:
	friend class UAuraCommerceSubsystem;

	void ApplyAuthorityPresentation(const FAuraMerchantPresentation& InPresentation);

	UPROPERTY(ReplicatedUsing = OnRep_Presentation, VisibleInstanceOnly, BlueprintReadOnly, Category = "Economy|Merchant", meta = (AllowPrivateAccess = "true"))
	FAuraMerchantPresentation Presentation;

	UFUNCTION()
	void OnRep_Presentation();
};
