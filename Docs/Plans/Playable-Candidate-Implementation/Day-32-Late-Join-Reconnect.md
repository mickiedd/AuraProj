# Day 32 — Late Join and Reconnect

Status: Planned  
Depends on: Day 31 persistence closure

## Goal

Treat joining an active world and returning after disconnect as normal player journeys, including full HUD replay.

## Work

- Verify role/identity, life state, ammo, battle phase, Civilian population, merchant stock, wallet/inventory, tutorial state, and persistence mode all converge for a late client.
- Make WebUI `ready` replay current state without requiring a new gameplay mutation.
- Clear and recreate focus, interaction, commerce, replay caches, delegates, timers, and browser state on disconnect/reconnect.
- Keep public provider-specific identity handling separate from local fixture identity.

## Detailed execution contract

### Files to inspect or modify

- **Join/reconnect:** `Source/Aura/Public/Game/GameServerClient.h`, `LoginPlayerController.h`, `LoadingPlayerController.h`, `AuraGameInstance.h`, `AuraPlayerController.h`, and matching implementations.
- **Replication/state:** `AuraPlayerState`, role/grant ledger, firearm/economy/persistence components, battle director, population manager, and merchant component.
- **Presentation:** `WebUIBridgeSubsystem`, HUD pages, focus/interaction/commerce replay handlers.
- **New planned output:** `Source/Aura/Private/Tests/AuraRoleBattleDay32Tests.cpp`, `RunPlayableCandidateDay32JoinReconnect.ps1`, and `day-32-join-reconnect.json`.

### Convergence contract

A late join receives the authoritative world snapshot and current battle/population/merchant state, then its own profile, role, ammo, economy, tutorial, and HUD snapshot. A reconnect creates a new connection/session nonce and restores only the last committed profile state. Replay is idempotent and never invokes gameplay mutation. Private wallet, inventory, ammo, and tutorial fields are owner-only; public world/member fields are shared.

### Detailed steps

1. Capture an authoritative state digest for role, life, ammo, battle phase, population, merchant stock, wallet/inventory revisions, tutorial, and persistence generation.
2. Join client 2 after combat/economy changes and before/after a world checkpoint; assert it reconstructs current state without triggering world load or reward/purchase.
3. Disconnect a client during attack, reload, commerce, death, and save; invalidate old session/request IDs and clear transient focus/delegate/timer/browser state.
4. Reconnect the same validated fixture identity with a new session and compare the restored profile to the last committed generation.
5. Drive `hud_ready`, browser reconnect, pawn replacement, and late-join replay repeatedly; assert one snapshot per lifecycle event.
6. Verify non-owner privacy on every state digest and that public provider identity code remains separate from local fixture identity.
7. Exercise listen and dedicated packaged lanes with delayed/reordered state events and no client mutation.

### Named automation and commands

- Native tests: `LateJoinWorldConvergence`, `ReconnectCommittedProfile`, `ReconnectNewSession`, `WorldLoadOnce`, `ReplayIdempotence`, `PawnReplacement`, `DisconnectDuringCommerce`, `PrivateStateIsolation`, and `BattlePhaseReplay`.
- Run `RunPlayableCandidateDay32JoinReconnect.ps1 -Mode Listen` and `-Mode Dedicated`; late join must be within 60 seconds and reconnect within 90 seconds.
- The report pairs before/after state digests, HUD captures, session IDs, persistence generation, logs, and result codes; any stale or duplicated state blocks Day 33.

## Validation and evidence

- Listen and dedicated: client 2 joins after battle/economy state changes, disconnects during combat and commerce, then reconnects.
- Capture before/after HUD screenshots and compare authoritative state, not just process success.
- Assert non-owner privacy after every reconnect.

## Deep-review closure

- **Owner surfaces:** replicated player/world state, the persistence reconciliation boundary, `WebUIBridgeSubsystem`, and focus/interaction/commerce replay state.
- **Required artifacts:** `day-32-join-reconnect.json`, paired before/after authoritative state snapshots, and packaged HUD captures for late join and reconnect.
- **Gate:** a late join reaches a usable HUD within 60 seconds of server readiness and a reconnect restores the last committed player state within 90 seconds; replay is idempotent, no duplicate grants/stock occur, and the second client never receives private wallet, inventory, or ammo data.

## Completion gate

Late join and reconnect produce the same usable, correctly scoped player experience as being present from startup, without stale or duplicated state.
