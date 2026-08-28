// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Economy/AuraEconomyTypes.h"
#include "AuraInventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FAuraInventoryChangedDelegate, const TArray<FAuraInventorySlot>& /*Slots*/, uint32 /*Revision*/);

UCLASS(ClassGroup = (Economy), meta = (BlueprintSpawnableComponent))
class AURA_API UAuraInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuraInventoryComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FAuraInventorySlot>& GetSlots() const { return Inventory.Items; }
	uint32 GetRevision() const { return Revision; }
	int64 GetItemQuantity(FName ItemId) const;
	bool HasItem(FName ItemId, int64 MinimumQuantity = 1) const;
	int64 GetUsedSlotCount() const { return Inventory.Items.Num(); }

	bool CanAddItem(FName ItemId, int64 Quantity) const;
	bool CanRemoveItem(FName ItemId, int64 Quantity) const;
	EAuraEconomyMutationResult CommitAddItem(FName ItemId, int64 Quantity, bool bBroadcastEvents = true);
	EAuraEconomyMutationResult CommitRemoveItem(FName ItemId, int64 Quantity, bool bBroadcastEvents = true);

	/** Authority-only replacement used by rollback-safe commerce/persistence owners. */
	bool RestoreInventoryState(const TArray<FAuraInventorySlot>& InSlots, uint32 InRevision, bool bBroadcastEvents = true);

	/** Publishes a previously silent authority mutation after an atomic transaction commits. */
	void PublishChanged() { BroadcastChanged(); }

	FAuraInventoryChangedDelegate OnInventoryChanged;

private:
	const FAuraItemDefinition* FindDefinition(FName ItemId) const;
	bool IsValidQuantity(int64 Quantity) const { return Quantity > 0; }
	void BroadcastChanged();

	UPROPERTY(ReplicatedUsing = OnRep_Inventory, VisibleInstanceOnly, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	FAuraInventoryList Inventory;

	UPROPERTY(ReplicatedUsing = OnRep_Revision, VisibleInstanceOnly, meta = (AllowPrivateAccess = "true"))
	uint32 Revision = 0;

	UFUNCTION()
	void OnRep_Inventory();

	UFUNCTION()
	void OnRep_Revision();
};
