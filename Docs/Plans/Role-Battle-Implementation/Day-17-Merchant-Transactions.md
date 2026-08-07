# Day 17 - Add Merchant Transactions

## Goal

Allow a player to buy an item from a Civilian merchant through an atomic, server-authoritative transaction.

## New files

- Source/Aura/Public/Economy/AuraMerchantComponent.h
- Source/Aura/Private/Economy/AuraMerchantComponent.cpp
- Source/Aura/Public/Economy/AuraCommerceSubsystem.h
- Source/Aura/Private/Economy/AuraCommerceSubsystem.cpp

## Files to modify

- Source/Aura/Public/Character/AuraCivilian.h
- Source/Aura/Private/Character/AuraCivilian.cpp
- Source/Aura/Public/Interaction/AuraInteractionComponent.h
- Source/Aura/Private/Interaction/AuraInteractionComponent.cpp
- Source/Aura/Public/Player/AuraPlayerController.h
- Source/Aura/Private/Player/AuraPlayerController.cpp

## Implementation steps

1. Attach AuraMerchantComponent only to Civilian actors with a Merchant economy profile.
2. Load offers by merchant ID from the validated economy registry.
3. Add a server RPC or validated interaction request for BuyOffer.
4. Validate:
   - Player is alive.
   - Merchant is alive and available.
   - Player is in range and line of sight.
   - Offer exists.
   - Stock is available.
   - Currency balance is sufficient.
   - Inventory has capacity.
5. Debit currency, decrement stock, and add item as one atomic server operation.
6. Reject duplicate or stale transaction requests using a request ID.
7. Return a result with success or a safe failure reason.
8. Add a basic merchant UI that displays replicated offers and prices.
9. Decide whether stock is per merchant instance or shared by merchant ID; use per-instance stock for the first slice.
10. Add buy tests for success, insufficient funds, full inventory, out-of-range, sold-out, and replayed request.

## Verification

- A player can buy a valid item.
- Currency and stock change exactly once.
- Failed purchases change nothing.
- Civilian death disables merchant interaction.
- A client cannot alter the price or item ID accepted by the server.

## Completion gate

The first business loop works end to end: Civilian merchant, interaction, server validation, currency debit, item grant, and UI result.

