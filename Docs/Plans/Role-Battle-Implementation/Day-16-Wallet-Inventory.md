# Day 16 - Add Wallet and Inventory

## Goal

Add server-authoritative currency and inventory state without coupling it to the Ability System Component.

## New files

- Source/Aura/Public/Economy/AuraCurrencyComponent.h
- Source/Aura/Private/Economy/AuraCurrencyComponent.cpp
- Source/Aura/Public/Economy/AuraInventoryComponent.h
- Source/Aura/Private/Economy/AuraInventoryComponent.cpp

## Files to modify

- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Player/AuraPlayerState.h
- Source/Aura/Private/Player/AuraPlayerState.cpp
- Source/Aura/Public/Economy/AuraEconomyTypes.h

## Implementation steps

1. Attach wallet and inventory to PlayerState or the persistent player owner, not the temporary pawn.
2. Replicate read-only state to clients.
3. Expose server-only mutation functions:
   - AddCurrency.
   - RemoveCurrency.
   - AddItem.
   - RemoveItem.
   - HasItem.
4. Add validation for negative values, overflow, invalid item IDs, stack limits, and capacity.
5. Add change delegates for UI updates.
6. Add a development-only grant command for test setup, protected from clients.
7. Initialize a small starting currency amount from EconomyConfig.
8. Keep inventory items as IDs and quantities; do not replicate arbitrary asset paths.
9. Add unit tests for stacking, capacity, debit, and invalid input.

## Verification

- Wallet replicates to the owning client.
- Inventory replicates valid item IDs and quantities.
- Negative and oversized mutations fail.
- Inventory state survives pawn death and respawn.
- A client cannot directly set currency or inventory.

## Completion gate

Player economy state belongs to the persistent player owner and is controlled only by validated server code.

