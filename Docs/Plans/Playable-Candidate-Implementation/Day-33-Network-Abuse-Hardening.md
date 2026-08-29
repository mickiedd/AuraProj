# Day 33 — Network Abuse and Error Hardening

Status: Planned  
Depends on: Days 25–32

## Goal

Close the security/error surface introduced by ammo, tutorial, reward, restock, and recovery work without redesigning the network model.

## Work

- Review every new and existing player-owned RPC, console path, Blueprint callable, and subsystem entry point.
- Include the Game Server Manager control plane: it is loopback-only by default, and any LAN/public binding requires an explicit allowlist plus a configured authentication token. An unauthenticated `request_server` must never start or expose a server.
- Enforce ownership, server-side target/range/state checks, rate limits, stale-reference handling, replay protection, and bounded rejection results.
- Ensure failed mutations emit no partial wallet, inventory, ammo, stock, save, or UI commit.
- Keep development fixtures and test hooks compile-gated and absent from Shipping.

## Detailed execution contract

### Files to inspect or modify

- **Player-owned endpoints:** `Source/Aura/Public/Player/AuraPlayerController.h/.cpp`, `AuraPlayerState.h/.cpp`, `Interaction/AuraInteractionComponent.h/.cpp`, and any reload/tutorial/commerce RPC declarations.
- **Authority surfaces:** `AuraCombatRules`, `AuraCombatStateComponent`, `AuraFireGun`, `AuraCommerceSubsystem`, `AuraPersistenceSubsystem`, population/merchant components, and Game Server Manager commands.
- **Build/security:** `Source/Aura.Target.cs`, `Source/AuraServer.Target.cs`, plugin module guards, `Config/DefaultGame.ini`, and Shipping staging/package scripts.
- **New output:** `Source/Aura/Private/Tests/AuraRoleBattleDay33Tests.cpp`, `RunPlayableCandidateDay33NetworkAbuse.ps1`, `day-33-negative-matrix.json`, and an RPC/command ownership table.

### Request-validation contract

Every client-originated action is documented as `endpoint → owner check → session/nonce → target/state/range → rate/replay → authoritative commit → result code`. Requests contain only stable IDs and client-observed intent; prices, quantities, damage, ammo result, role, stock, save, and success values are server-resolved. The Game Server Manager request must additionally pass control-plane authentication and level allowlist checks. A rejected request is side-effect free and produces a bounded typed result.

### Detailed steps

1. Enumerate all RPCs, reliable/unreliable commands, Blueprint-callable functions, console commands, WebUI bridge messages, Game Server Manager control messages, test hooks, grant helpers, and subsystem entry points introduced or touched by Days 25–32.
2. Record endpoint owner, authority process, required session/nonce, validation order, mutation set, result code, replication visibility, and Shipping availability for each action.
3. Add server checks for role/loadout, Alive state, target identity/range/LOS, ownership, cadence, replay/request sequence, merchant binding, stock, wallet/inventory capacity, and persistence generation.
4. Make multi-field mutations transactional: use before-images or equivalent rollback and publish replication/UI only after complete commit.
5. Return stable rejection codes for forged target/member IDs, ammo/reload, rapid-fire, invalid role, merchant replay, stale nonce, non-owner, disconnect, and stale-save cases.
6. Add compile/runtime guards so development probes, fixture identities, grants, cheats, and test commands are absent from Shipping.
7. Run negative cases under both topologies, with delayed/reordered input and one valid request after each rejection to prove recovery.
8. Scan packaged Shipping outputs and archive the ownership table before Day 34. Verify that the Shipping/server-operations configuration cannot expose an unauthenticated GSM control socket.

### Named automation and commands

- Native tests: `RPCOwnership`, `ForgedTarget`, `ForgedAmmo`, `ReloadSpam`, `RapidFire`, `InvalidRole`, `MerchantReplay`, `StaleNonce`, `NonOwnerAccess`, `DisconnectDuringCommit`, `NoPartialMutation`, and `ShippingSurfaceGuard`.
- Run `RunPlayableCandidateDay33NetworkAbuse.ps1 -Mode Listen` and `-Mode Dedicated` with two clients and the existing Day 17–19 negative/replay coverage.
- Every negative case must return a result within 2 seconds, mutate zero authoritative fields, and leave the next valid request usable; any silent timeout, duplicate mutation, or private read is P0.

## Validation and evidence

- Negative tests cover fabricated target/member IDs, forged ammo, reload spam, rapid-fire, invalid role, merchant replay, stale nonce, non-owner access, and disconnect during commit.
- Run both listen and dedicated two-client negative matrices.
- Scan Shipping outputs for test/grant/cheat/probe surfaces.

## Deep-review closure

- **Owner surfaces:** every new RPC/command in the ammo, tutorial, economy, recovery, and reconnect paths, plus the Shipping target configuration and existing probe compile guards.
- **Required artifacts:** `day-33-negative-matrix.json`, an RPC/command ownership table, rejection-result catalog, and Shipping scan report.
- **Gate:** each negative case returns a stable rejection code within 2 seconds, performs zero partial mutation, and leaves the next valid request usable; a Shipping scan finds no test grant, cheat, probe, or development-only endpoint. Any duplicate mutation, cross-owner read, or silent timeout is P0.

## Completion gate

Untrusted client input cannot manufacture damage, ammo, roles, currency, items, stock, persistence, or privileged visibility.
