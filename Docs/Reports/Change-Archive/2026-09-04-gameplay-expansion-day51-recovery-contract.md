# Gameplay Expansion Day 51 — cooperative recovery contract foundation

Date: 2026-09-04. Scope: the next dependency-safe source slice in the Gameplay Expansion Days 41–60 plan. This record covers the inert Day 51 recovery transaction contract; it does not claim the full playable recovery runtime gate.

[Visual summary](2026-09-04-gameplay-expansion-day51-recovery-contract.svg) · [Day 51 plan](../../Plans/Gameplay-Expansion-Implementation/Day-51-Cooperative-Recovery.md) · [final native log](../../../Saved/Logs/GameplayDay51Final.log)

## Intent

Define and validate cooperative recovery authority, charge, attachment, retained-state, reconnect, proxy, and wipe rules without modifying the existing combat life enum or prematurely wiring production death, respawn, possession, ASC, save, mission, or HUD paths.

## Changed behavior

- Added an authority-only, value-based recovery reducer scoped exclusively to `GameplayExpansionV1`; legacy profiles do not opt in.
- Added exactly-once death sequence and deterministic beacon handling without adding a global Downed state. Members move through run-local Alive, AwaitingRescue, Recovering, and Forfeited states.
- Added a three-second channel and atomic recovery receipt. Pair runs share two charges; solo has one checkpoint charge. Concurrent completions reuse one live receipt.
- A charge is reserved before pawn/ASC/loadout verification and spent only after every attachment succeeds. The one allowed retry is delayed by one second; terminal failure releases the reservation, consumes no charge, publishes no Alive state, and invokes no legacy save.
- Successful recovery publishes 35% health and 50% mana while preserving completed ammunition, augments, supplies, and absolute ability/evade/supply cooldown deadlines.
- Reconnect does not heal or resurrect. Repeated disconnect cannot extend the 60-second proxy lease or reset retained combat state; accepted proxy death and committed expiry recompute wipe.
- Team wipe fails exactly once even with charges available, releases uncommitted reservations, and expires orphan beacons. Solo second death fails after its single charge is consumed.
- Extended `GameplayCombatTuning.json` with a closed `CONTRACT_ONLY` / `UNWIRED` recovery section and added all nine named native tests plus static legacy-isolation checks.

## Validation

- `./BuildEditor.command`: UE 5.5 Mac Development compile, link, and deployment passed. The pre-existing `CompressImageArray` deprecation remains non-blocking.
- Native `Aura.Gameplay`: 66/66 passed, 0 failed, including Day 51 9/9. Evidence: [`GameplayDay51Final.log`](../../../Saved/Logs/GameplayDay51Final.log).
- Day 51 offline contract suite: 3/3 passed.
- Dependency offline suites for Days 42–51: 35/35 passed.
- Death dedupe, charge races, retained state, reconnect, attachment failure/retry, cooldown-preserving disconnect, proxy death/expiry, reserved-charge wipe, same-tick wipe, solo last-charge failure, and legacy isolation are covered.
- Unreal startup continues to report the known Crunch Git-LFS pointer assets as unloadable. Those unrelated payloads were not changed or masked.

## Remaining gates

The full Day 51 completion gate remains open: production death adapter, recovery component/beacon actor, surveyed safe anchor, mission interaction lease, possession-time profile restoration, ASC verification, HUD/RPC presentation, packaged multiplayer traces, and visible teammate-rescue evidence are still required. `CONTRACT_ONLY`, `UNWIRED`, `DAY40_PACKAGED_EVIDENCE_MISSING`, `DAY42_POSSESSION_RUNTIME_UNIMPLEMENTED`, `DAY49_LIVE_OBJECTIVE_LOOP_UNIMPLEMENTED`, and `ANCHOR_SURVEY_UNVERIFIED` remain fail-closed.

The required illustration is [archived here](2026-09-04-gameplay-expansion-day51-recovery-contract.svg).
