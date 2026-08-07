# Day 15 - Add Economy Definitions

## Goal

Define item, offer, merchant, and currency data before adding runtime transactions.

## New files

- Content/Config/ItemDefinitions.json
- Content/Config/MerchantDefinitions.json
- Content/Config/EconomyConfig.json
- Source/Aura/Public/Economy/AuraEconomyTypes.h
- Source/Aura/Private/Economy/AuraEconomyTypes.cpp
- Source/Aura/Public/Economy/AuraEconomyConfig.h
- Source/Aura/Private/Economy/AuraEconomyConfig.cpp

## Implementation steps

1. Define item fields:
   - Item ID.
   - Display name.
   - Category.
   - Stack limit.
   - Sell value.
   - Gameplay effect or use action, if applicable.
2. Define offer fields:
   - Offer ID.
   - Item ID.
   - Buy price.
   - Sell price, if supported.
   - Stock.
   - Required role or interaction tag, if needed.
3. Define merchant fields:
   - Merchant ID.
   - Civilian role/work profile.
   - Offer list.
   - Stock policy.
   - Open schedule placeholder.
4. Define currency ID and starting player balance.
5. Add safe parsing and validation similar to RoleConfig.
6. Reject duplicate IDs, missing items, invalid prices, invalid stack limits, and missing merchant offers.
7. Add one simple civilian merchant with two offers.
8. Do not modify save data or runtime inventory yet.

## Verification

- All three config files load at startup.
- Invalid definitions produce readable errors.
- Merchant offers reference valid items.
- The test merchant can be found by ID.

## Completion gate

Economy data is validated and available through a read-only runtime registry.

