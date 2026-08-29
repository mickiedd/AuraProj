# Day 29 — Reward-to-Merchant Loop

Status: Planned  
Depends on: Day 24 tutorial, Day 26 HUD, and Days 16–17 economy contracts

## Goal

Make the five-minute session loop meaningful: an accepted combat outcome changes an authoritative value that the player can see and spend.

## Work

- Use the Day 21 manifest-bound server-owned `RoleBattle.CivilianLethalReward` definition for the supported fixture: `25 gold` for one authoritative lethal Civilian outcome attributed to the attacking player. If the definition is absent or differs from the manifest, fail the content gate; never create or infer a reward at runtime from actor tags or client claims.
- Connect reward → wallet/inventory revision → merchant offer/stock → purchase result → HUD update.
- Display success, insufficient funds, sold out, invalid target, and replayed transaction outcomes clearly.
- Keep reward and purchase mutations atomic and compatible with existing persistence and owner-only replication.

## Detailed execution contract

### Files to inspect or modify

- **Combat outcome owner:** the Day 11 death/policy dispatcher, Day 12 battle-zone rules, Day 13 population member state, and the active FireGun attribution path.
- **Economy owner:** `Source/Aura/Public/Economy/AuraEconomyTypes.h`, `AuraCurrencyComponent.h`, `AuraInventoryComponent.h`, `AuraMerchantComponent.h`, `AuraCommerceSubsystem.h`, and matching implementations.
- **Content/UI:** `Content/Config/ItemDefinitions.json`, `MerchantDefinitions.json`, `EconomyConfig.json`, the existing merchant widget/controller, and WebUI merchant/HUD pages.
- **New planned output:** `Content/Config/RewardDefinitions.json` with `RoleBattle.CivilianLethalReward`, `Docs/Reference/Playable-Candidate-Reward-Transaction-Contract.md`, `Source/Aura/Private/Tests/AuraRoleBattleDay29Tests.cpp`, `RunPlayableCandidateDay29RewardMerchant.ps1`, and `day-29-reward-merchant.json`.

### Reward and transaction contract

`RoleBattle.CivilianLethalReward` contains a stable reward ID, the manifest-bound authoritative Civilian lethal outcome, `gold`/`25`, and a revision. Each eligible outcome receives a server-issued `OutcomeCorrelationId`; one reward transaction can commit at most once per ID. The canonical purchase is `market_health_potion` for `25 gold`, granting one `health_potion`, on the normalized merchant ID `marketmerchant`. The purchase request contains only the owning session/request ID, registered merchant, and offer ID; price, quantity, item, reward, and success are server-resolved. Reward and purchase publish PlayerState/merchant revisions only after atomic commit.

### Detailed steps

1. Trace authoritative Civilian/battle death attribution and bind it to the Day 21 manifest outcome; prove ordinary damage, client tags, and client claims cannot emit rewards.
2. Add and validate the minimal reward definition; require the manifest-bound positive bounded amount and references to existing currency/item/merchant definitions. A missing or mismatched definition is a validation failure, not an implementation-time choice.
3. Connect one accepted outcome to the existing currency/inventory mutation APIs with an at-most-once outcome ID and no UI/latent callback between mutations.
4. Bind the reward to the existing stable `PopulationMemberId` merchant and one configured offer whose price is payable by the fixture reward.
5. Route purchase through the existing player-owned interaction/commerce endpoint; revalidate owner, Alive state, distance/LOS, merchant identity, offer, stock, eligibility, and balance.
6. Return explicit success/insufficient/sold-out/invalid/replayed results and render them without replacing replicated authoritative values from cached responses.
7. Test reward-before-purchase, purchase-before-reward, simultaneous last-stock contention, replay, disconnect during commit, death during commerce, and reconnect restoration.
8. Capture one human walkthrough from combat result through inventory inspection and merchant purchase for both Aura and BungeeMan lanes.

### Named automation and commands

- Native tests: `RewardDefinition`, `EligibleOutcome`, `RewardAtMostOnce`, `PurchaseAtomic`, `InsufficientFunds`, `SoldOut`, `ForgedPayloadIgnored`, `ReplayIdempotence`, `DisconnectDuringCommit`, and `OwnerPrivacy`.
- Run `RunPlayableCandidateDay29RewardMerchant.ps1 -Mode Listen` and `-Mode Dedicated` with two clients and the existing Day 17 transaction tests as regression.
- The artifact records before/after wallet, inventory, stock, reward/purchase correlation IDs, revisions, logs, screenshots, and exit code; any partial mutation blocks Day 30.

## Validation and evidence

- Run earn → focus merchant → purchase → inspect inventory → reconnect, for both a valid and a failed purchase.
- Test simultaneous purchase/reward ordering and replayed requests.
- Retain authoritative logs and a visual walkthrough.

## Deep-review closure

- **Owner surfaces:** `Source/Aura/Public/Economy/AuraCurrencyComponent.h`, `AuraInventoryComponent.h`, `AuraMerchantComponent.h`, `AuraCommerceSubsystem.h`, the existing `Content/Config/MerchantDefinitions.json`, and the authoritative Civilian/battle outcome boundary.
- **Required artifacts:** the `RoleBattle.CivilianLethalReward` content record, `Docs/Reference/Playable-Candidate-Reward-Transaction-Contract.md`, and `day-29-reward-merchant.json` with reward, wallet, inventory, stock, purchase, and correlation revisions.
- **Gate:** one authoritative accepted outcome grants the configured positive reward at most once; the test purchase costs no more than that reward; simultaneous reward/purchase and replayed requests are atomic and idempotent; client-supplied amounts, item IDs, and success flags are ignored.

## Completion gate

A new player can complete one visible combat-to-spend loop, and no client can manufacture currency, inventory, reward, or a successful purchase.

## Defer

Do not build a full loot table, crafting, quest rewards, or reputation economy.
