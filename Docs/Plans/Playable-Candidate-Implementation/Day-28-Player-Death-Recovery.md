# Day 28 — Player Death and Recovery

Status: Planned  
Depends on: Day 23 boot flow, Day 26 HUD, and existing Day 11 death policies

## Goal

Make player death a supported game state with clear loss of control, recovery, and state restoration.

## Work

- Trace authoritative Player → Dying → Dead → respawn/re-entry, including controller, pawn, ASC, role grant ledger, ammo, HUD, focus, and input ownership.
- Add or repair player-facing death, respawn, and recovery states without reusing Civilian population death behavior.
- Preserve wallet/inventory and the explicitly chosen ammo/tutorial persistence policy across pawn replacement.
- Ensure stale target, merchant, browser, delegate, and timer references are cleared.

## Detailed execution contract

### Files to inspect or modify

- **Life/death authority:** `Source/Aura/Public/Combat/AuraCombatStateComponent.h/.cpp`, `AuraCombatRules.h/.cpp`, `Source/Aura/Public/Character/AuraCharacterBase.h/.cpp`, `AuraCharacter.h/.cpp`, and the existing Day 11 death-policy dispatcher.
- **Player ownership:** `Source/Aura/Public/Player/AuraPlayerController.h/.cpp`, `AuraPlayerState.h/.cpp`, ASC/grant ledger, pawn replacement, and persistence hooks.
- **Presentation/evidence:** `Source/Aura/Public/UI/HUD/AuraHUD.h/.cpp`, WebUI bridge/pages, `Source/Aura/Private/Tests/AuraRoleBattleDay28Tests.cpp`, `RunPlayableCandidateDay28DeathRecovery.ps1`, and `day-28-death-recovery.json`.

### Life-state contract

Player state transitions are `Alive → Dying → Dead → Recovering → Alive`; Civilian population death remains a separate `Death.PopulationRespawn` policy. Only the authoritative server changes life state, controller possession, role/grant ledger, ammo recovery, and persistence checkpoint. A committed `Dead` or `Recovering` player profile is normalized to one server-owned `Recovering → Alive` transition on the next valid join/restart. Clients receive a state snapshot and result reason; they cannot request a direct respawn or clear death.

### Detailed steps

1. Trace the Day 11 exactly-once fatal-damage path and add a player-specific policy adapter without changing Civilian or enemy dispatch.
2. Define the transition guards, replicated state, terminal result codes, and ownership of controller, pawn, ASC, target, merchant, browser, delegate, and timer cleanup.
3. On lethal authoritative damage, stop input/attack/reload, close interaction/commerce, clear focus and transient delegates, publish one Dying/Dead sequence, cancel any in-flight reload, and persist only the frozen committed player state.
4. Implement one server-controlled recovery path that creates/reuses the correct role shell, restores PlayerState/ASC ledger once, applies the frozen ammo/tutorial/wallet/inventory policy, and rebinds HUD/input once.
5. Reject client respawn/recovery, duplicate death, stale pawn, old transaction, and post-death attack requests with typed results.
6. Test remote observation, death during reload/commerce, simultaneous death and reward, pawn replacement twice, reconnect during Dead/Recovering, and owner/non-owner HUD visibility.
7. Run packaged visual captures and inspect logs for one transition, one cleanup, one grant ledger, and one recovery per cycle.

### Named automation and commands

- Native tests: `Aura.RoleBattle.Day28.PlayerDeathExactlyOnce`, `CivilianPolicyUnchanged`, `DeathStopsInput`, `RecoveryRestoresRole`, `RecoveryNoDuplicateGrant`, `DeathDuringReload`, `DeathDuringCommerce`, and `ReconnectDuringRecovery`.
- Run `RunPlayableCandidateDay28DeathRecovery.ps1 -Mode Listen` and `-Mode Dedicated` for both roles; perform three death/recovery cycles per lane.
- Any stuck pawn/controller/HUD, duplicated grant, leaked focus/delegate/timer, cross-owner state, or nonzero/missing artifact blocks Day 29.

## Validation and evidence

- Kill a player from an authoritative server in listen and dedicated two-client fixtures.
- Verify remote observation, respawn role/loadout, HUD replay, input recovery, and no duplicate grants.
- Capture a packaged visual death/recovery run.

## Deep-review closure

- **Owner surfaces:** existing player life-state/death policy, role grant ledger, ASC/pawn replacement path, persistence boundary, and the Day 26 WebUI state replay.
- **Required artifacts:** `Docs/Reference/Playable-Candidate-Life-State-Contract.md`, `day-28-death-recovery.json`, and a packaged capture showing death, loss of control, respawn, and restored HUD.
- **Gate:** three authoritative death/recovery cycles per role and topology produce one death transition and one recovery per cycle, preserve the frozen wallet/inventory/ammo/tutorial policy, and leave no stale focus, delegate, timer, browser, controller, or duplicate grant.

## Completion gate

A player can die, understand what happened, recover, and continue without a stuck pawn/controller/HUD or duplicated state.

## Defer

Do not add permadeath, revive classes, or a new reward framework.
