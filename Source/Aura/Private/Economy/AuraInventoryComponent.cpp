// Copyright Druid Mechanics

#include "Economy/AuraInventoryComponent.h"

#include "Economy/AuraEconomyRegistrySubsystem.h"
#include "Aura/AuraLogChannels.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

namespace AuraInventoryComponentPrivate
{
	const UAuraEconomyRegistrySubsystem* GetRegistry(const UObject* Object)
	{
		const UWorld* World = Object ? Object->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UAuraEconomyRegistrySubsystem>() : nullptr;
	}

	bool AddQuantityChecked(int64 A, int64 B, int64& Out)
	{
		if (A < 0 || B < 0 || B > MAX_int64 - A) return false;
		Out = A + B;
		return true;
	}
}

UAuraInventoryComponent::UAuraInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UAuraInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UAuraInventoryComponent, Inventory, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UAuraInventoryComponent, Revision, COND_OwnerOnly);
}

const FAuraItemDefinition* UAuraInventoryComponent::FindDefinition(FName ItemId) const
{
	const UAuraEconomyRegistrySubsystem* Registry = AuraInventoryComponentPrivate::GetRegistry(this);
	return Registry && Registry->IsReady() && !ItemId.IsNone() ? Registry->FindItem(ItemId) : nullptr;
}

int64 UAuraInventoryComponent::GetItemQuantity(FName ItemId) const
{
	int64 Total = 0;
	for (const FAuraInventorySlot& Slot : Inventory.Items)
	{
		if (Slot.ItemId == ItemId && Slot.Quantity > 0 && Slot.Quantity <= MAX_int64 - Total) Total += Slot.Quantity;
	}
	return Total;
}

bool UAuraInventoryComponent::HasItem(FName ItemId, int64 MinimumQuantity) const
{
	return MinimumQuantity > 0 && GetItemQuantity(ItemId) >= MinimumQuantity;
}

bool UAuraInventoryComponent::CanAddItem(FName ItemId, int64 Quantity) const
{
	const FAuraItemDefinition* Definition = FindDefinition(ItemId);
	if (!Definition || !IsValidQuantity(Quantity) || Definition->StackLimit <= 0) return false;
	const UAuraEconomyRegistrySubsystem* Registry = AuraInventoryComponentPrivate::GetRegistry(this);
	const int64 MaximumSlots = Registry->GetSnapshot()->Settings.MaximumItemSlots;
	if (MaximumSlots <= 0 || Inventory.Items.Num() > MaximumSlots) return false;

	int64 Remaining = Quantity;
	for (const FAuraInventorySlot& Slot : Inventory.Items)
	{
		if (Slot.ItemId != ItemId) continue;
		if (Slot.Quantity <= 0 || Slot.Quantity > Definition->StackLimit) return false;
		const int64 Room = Definition->StackLimit - Slot.Quantity;
		Remaining = FMath::Max<int64>(0, Remaining - Room);
		if (Remaining == 0) return true;
	}

	const int64 FreeSlots = MaximumSlots - Inventory.Items.Num();
	if (FreeSlots <= 0) return false;
	if (Remaining > MAX_int64 - (Definition->StackLimit - 1)) return false;
	const int64 NeededSlots = (Remaining + Definition->StackLimit - 1) / Definition->StackLimit;
	return NeededSlots > 0 && NeededSlots <= FreeSlots;
}

bool UAuraInventoryComponent::CanRemoveItem(FName ItemId, int64 Quantity) const
{
	return FindDefinition(ItemId) && IsValidQuantity(Quantity) && GetItemQuantity(ItemId) >= Quantity;
}

EAuraEconomyMutationResult UAuraInventoryComponent::CommitAddItem(FName ItemId, int64 Quantity, bool bBroadcastEvents)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return EAuraEconomyMutationResult::NotAuthority;
	if (!IsValidQuantity(Quantity)) return EAuraEconomyMutationResult::InvalidAmount;
	const FAuraItemDefinition* Definition = FindDefinition(ItemId);
	if (!Definition) return EAuraEconomyMutationResult::InvalidDefinition;
	if (!CanAddItem(ItemId, Quantity)) return EAuraEconomyMutationResult::InventoryFull;

	TArray<FAuraInventorySlot> Candidate = Inventory.Items;
	int64 Remaining = Quantity;
	for (FAuraInventorySlot& Slot : Candidate)
	{
		if (Slot.ItemId != ItemId) continue;
		const int64 Room = Definition->StackLimit - Slot.Quantity;
		const int64 Added = FMath::Min(Remaining, Room);
		int64 NewQuantity = 0;
		if (!AuraInventoryComponentPrivate::AddQuantityChecked(Slot.Quantity, Added, NewQuantity)) return EAuraEconomyMutationResult::Overflow;
		Slot.Quantity = NewQuantity;
		Remaining -= Added;
		if (Remaining == 0) break;
	}
	while (Remaining > 0)
	{
		FAuraInventorySlot& NewSlot = Candidate.AddDefaulted_GetRef();
		NewSlot.ItemId = ItemId;
		NewSlot.Quantity = FMath::Min(Remaining, Definition->StackLimit);
		Remaining -= NewSlot.Quantity;
	}
	Inventory.Items = MoveTemp(Candidate);
	Inventory.MarkArrayDirty();
	++Revision;
	if (bBroadcastEvents) BroadcastChanged();
	return EAuraEconomyMutationResult::Success;
}

EAuraEconomyMutationResult UAuraInventoryComponent::CommitRemoveItem(FName ItemId, int64 Quantity, bool bBroadcastEvents)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return EAuraEconomyMutationResult::NotAuthority;
	if (!IsValidQuantity(Quantity)) return EAuraEconomyMutationResult::InvalidAmount;
	if (!FindDefinition(ItemId)) return EAuraEconomyMutationResult::InvalidDefinition;
	if (!CanRemoveItem(ItemId, Quantity)) return EAuraEconomyMutationResult::InsufficientItems;

	TArray<FAuraInventorySlot> Candidate = Inventory.Items;
	int64 Remaining = Quantity;
	for (int32 Index = Candidate.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FAuraInventorySlot& Slot = Candidate[Index];
		if (Slot.ItemId != ItemId) continue;
		const int64 Removed = FMath::Min(Remaining, Slot.Quantity);
		Slot.Quantity -= Removed;
		Remaining -= Removed;
		if (Slot.Quantity == 0) Candidate.RemoveAt(Index);
	}
	if (Remaining != 0) return EAuraEconomyMutationResult::InvalidState;
	Inventory.Items = MoveTemp(Candidate);
	Inventory.MarkArrayDirty();
	++Revision;
	if (bBroadcastEvents) BroadcastChanged();
	return EAuraEconomyMutationResult::Success;
}

bool UAuraInventoryComponent::RestoreInventoryState(const TArray<FAuraInventorySlot>& InSlots, uint32 InRevision, bool bBroadcastEvents)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
	const UAuraEconomyRegistrySubsystem* Registry = AuraInventoryComponentPrivate::GetRegistry(this);
	if (!Registry || !Registry->IsReady() || InSlots.Num() > Registry->GetSnapshot()->Settings.MaximumItemSlots) return false;
	TArray<FAuraInventorySlot> Candidate;
	for (const FAuraInventorySlot& Slot : InSlots)
	{
		const FAuraItemDefinition* Definition = FindDefinition(Slot.ItemId);
		if (Slot.ItemId.IsNone() || !Definition || Slot.Quantity <= 0 || Slot.Quantity > Definition->StackLimit)
		{
			return false;
		}
		int64 ExistingQuantity = 0;
		for (const FAuraInventorySlot& Existing : Candidate)
		{
			if (Existing.ItemId == Slot.ItemId)
			{
				if (!AuraInventoryComponentPrivate::AddQuantityChecked(ExistingQuantity, Existing.Quantity, ExistingQuantity)) return false;
			}
		}
		if (!AuraInventoryComponentPrivate::AddQuantityChecked(ExistingQuantity, Slot.Quantity, ExistingQuantity)) return false;
		Candidate.Add(Slot);
	}
	Inventory.Items = MoveTemp(Candidate);
	Inventory.MarkArrayDirty();
	Revision = InRevision;
	if (bBroadcastEvents) BroadcastChanged();
	return true;
}

void UAuraInventoryComponent::BroadcastChanged()
{
	OnInventoryChanged.Broadcast(Inventory.Items, Revision);
	if (AActor* OwnerActor = GetOwner()) OwnerActor->ForceNetUpdate();
}

void UAuraInventoryComponent::OnRep_Inventory() {}
void UAuraInventoryComponent::OnRep_Revision()
{
	const APlayerState* OwnerPlayerState = Cast<APlayerState>(GetOwner());
	const FString OwnerPlayerName = OwnerPlayerState ? OwnerPlayerState->GetPlayerName() : FString(TEXT("<unknown>"));
	UE_LOG(LogAura, Log, TEXT("[Economy][Client] Player=%s Inventory replicated slots=%d revision=%u."),
		*OwnerPlayerName, Inventory.Items.Num(), Revision);
	BroadcastChanged();
}
