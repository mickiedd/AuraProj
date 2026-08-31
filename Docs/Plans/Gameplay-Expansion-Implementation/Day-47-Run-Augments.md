# Day 47 — Run Augments

Status: Planned; not implemented by this planning job.  
Depends on: Day 46 role/status/minimal medkit and emergency-resource interfaces, Day 43 two-cell boundary events and Day 42 profile isolation.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Two short build choices produce noticeable changes in how a run plays.

## Exact change surfaces

- New: `Content/Config/GameplayAugmentDefinitions.json`; `Source/Aura/Public/Gameplay/AuraAugmentOfferTypes.h`, `AuraRunAugmentComponent.h` and implementation.
- Extend Day 42 run loadout/snapshot, Day 43 mission CellCompleted flow, Day 46 effect/cost interfaces and AuraRunSupplyComponent medkit healing parameter.
- Existing: `Source/Aura/Private/Player/AuraPlayerController.cpp`, native overlay controller, `Plugins/AuraWebUI/Content/WebUI/hud-bottom.html`, generic ability capture in `Private/Game/AuraPersistenceSubsystem.cpp`.

## Data and authority contract

Exactly the eight architecture-defined augments; eligible pool is four shared plus two for the owner's role. Each boundary offers three distinct unchosen definitions; two choices per run. Server samples seeded offers and stores offer revision, expiry and choice receipt. Deadline 20s auto-selects first still-valid listed option. Offers are owner-only; permanent wallet and other owner's offers are never included. Run-scoped effects modify parameters through bounded interfaces, not arbitrary gameplay JSON execution.

Both choices consume Day 43's already implemented first/second CellCompleted events. field_medic modifies the real Day 46 medkit consumer from 35% to 45% maximum health without adding charges. steady_hands proposes four-second Exposed expiry only for that owner's application; the Day 46 later-expiry/lower-sequence selection rule determines ownership and cannot shorten or mutate another selected application directly. No modifier depends on a Day 52 placeholder consumer.

## Numbered implementation steps

1. Validate unique IDs, eligible roles, effect parameter whitelist, bounds and conflict rules; reject a pool with fewer than three eligible unchosen entries before run entry.
2. Consume the two existing generation-bound CellCompleted events to produce one offer per boundary and pause new encounter admission during the choice window; run deadline continues. Duplicate/stale boundaries cannot grant extra choices. Store choices server-side before returning success.
3. Implement all eight modifiers exactly as listed in architecture and keep their baseline/modified values visible in the choice card. Apply field_medic to the Day 46 medkit consumer, and exercise steady_hands through actual competing-owner Exposed applications using the selected record's expiry/attribution rules.
4. Track each granted effect handle/parameter override by run and choice. Restore original values on exit/death-reattach correctly; never remove unrelated profile effects.
5. Handle reconnect and duplicate choice by returning the same stored offer/result. Applying a second request at an already committed revision cannot stack effects.
6. Compare two specific builds per role in the same fixed encounter and ask players to explain the difference before expanding content.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay47Tests.cpp`; namespace `Aura.Gameplay.Day47`.

- OfferEligibilityAndUniqueness — three different eligible choices, no already-owned entry.
- ChoiceReplayNoStack — duplicated/out-of-order requests commit one modifier.
- ReconnectKeepsOffer — reopen/reconnect cannot reroll or extend deadline.
- TimeoutChoosesVisibleDefault — after 20s exactly the first visible valid option commits.
- AllEightEffectBounds — assert the architecture's radius/cooldown/target/heal/status limits.
- ExistingBoundariesGrantTwoOffers — the two actual Day 43 cell completions produce exactly two offers; duplicate/stale events grant none.
- AugmentedExposureOwnership — real steady_hands and unaugmented applications select later expiry then lower sequence without shortening duration or changing another selected application directly.
- FieldMedicUsesRunMedkit — a real Day 46 medkit heals 45% instead of 35%, remains capped at maximum, consumes one existing charge and cannot add charges.
- RunExitRestoresBaseline — ten choice/abort cycles leave no effect/spec/parameter growth.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 47 -Stage Fast -RunId d47-fast
./RunGameplayExpansion.ps1 -Day 47 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d47-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

AugmentLab: fixed seeds 41001–41003, actual first/second cell completion plus duplicate/stale boundary inputs, both roles, same-role and mixed owners, competing augmented/unaugmented Exposed applications, real medkit use, reconnect at 19s, selection racing timeout at 20s.

## Failure and timeout semantics

Malformed/undersized pools block entry. A selection after expiry returns the already committed auto-choice. Mid-run definition mismatch aborts; no silent reroll, hidden extra choice or broad effect cleanup. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-47.json, offer-seed-sequences.json, owner-privacy-capture.json, modifier-before-after.csv, paired-build observation notes and choice-card captures. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All eight modifications work through implemented consumers within limits, actual two-cell boundaries grant exactly two choices, choices stay distinct and retained for same-server reconnect, and players can identify at least one tactical effect from each tested build. field_medic and steady_hands have real medkit/competing-owner evidence rather than placeholder parameter assertions.

## Defer / anti-goals

No rarity, loot boxes, weighted monetization, unlimited rerolls, permanent skill tree or content count inflated by statistical permutations.
