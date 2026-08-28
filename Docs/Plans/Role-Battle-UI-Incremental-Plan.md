# Role/Battle UI Incremental Plan

Status: implemented for the player-facing Day 01–18 slice on 2026-08-28.

## Purpose

The Day 01–18 implementation added authoritative role, combat, Civilian, battle, interaction, economy, and persistence systems. This plan maps each system change to a visible player signal so the player can understand what changed without relying on logs or developer tools.

The UI remains WebUI-only and local-player-owned. Native gameplay code remains the source of truth: the HUD only displays replicated state and routes explicit, validated commands back through the existing player-owned interaction endpoint.

## Day-by-day coverage

| Day | Implemented system change | Player-facing UI signal | Surface |
| --- | --- | --- | --- |
| 01 | BungeeMan baseline and Gun skill path | Equipped ability slot shows the ability name, icon/fallback, input key, and equipped state | Bottom HUD |
| 02 | Replicated combat identity | Current role, combat profile, faction, and identity availability are visible | Left-top HUD |
| 03 | Life state and combat-rule foundation | Player life badge and target combat status distinguish combat-ready from protected targets | Left-top and bottom HUD |
| 04 | Authoritative damage boundary | Target preview exposes the resolved attack permission without implying that a browser click can apply damage | Bottom HUD |
| 05 | Versioned role schema | The role/profile identity shown by the HUD comes from the applied role state rather than a hard-coded label | Left-top HUD |
| 06 | Deterministic role application and loadout | Ability slots replay after pawn replacement and show the active loadout, including FireGun | Bottom HUD |
| 07 | Listen/dedicated/package regression | The same bounded three-panel HUD contract is used in every supported topology | All gameplay HUD panels |
| 08 | Replicated Civilian actor | A Civilian target shows its relationship, life state, and distinct non-combat interaction context | Bottom HUD |
| 09 | Stable population slots | Battle summary shows active civilians versus configured maximum; target details include stable member and zone IDs | Left-top and bottom HUD |
| 10 | Civilian work/observe/flee/shelter AI | Current Civilian activity is shown while the target is focused | Bottom HUD |
| 11 | Exactly-once death policies | Life state, target clearing, and population casualty count communicate death without duplicate UI actions | All relevant panels |
| 12 | Battle phases, zones, and event identity | Current phase and active event identity are always visible | Left-top HUD |
| 13 | Corpse cleanup and deterministic refill | Active/max/pending-refill and casualty summary update as slots die and refill; focused dead targets stop offering stale interaction | Left-top and bottom HUD |
| 14 | Separate targeting and Interact | Target relationship/options and the explicit Interact button remain separate; Trade opens a merchant panel only after the native target/option check | Bottom and right-top HUD |
| 15 | Economy registry | Registry-resolved item names, quantities, prices, stock policy, and availability are rendered | Right-top HUD |
| 16 | Owner-only wallet/inventory | A Bag action exposes the local player's wallet balance and owner-only inventory with revision-backed updates | Right-top HUD |
| 17 | Replay-safe merchant transactions | Merchant identity, offers, stock, pending state, and authoritative result errors/success are visible | Right-top HUD |
| 18 | Player/world persistence | The HUD distinguishes a restored persistent profile from a session profile and replays restored economy state; world population remains represented by the replicated battle summary | Left-top and right-top HUD |

Days 19–20 are multiplayer hardening and release evidence milestones. They do not add a new player-facing mechanic, but they validate the same UI event contract across listen, dedicated, packaged, and late-join conditions.

## WebUI event contract

| Event | Producer | Consumer | Meaning |
| --- | --- | --- | --- |
| `hud_role_state` | `AAuraHUD` | Left-top panel | Applied role, combat identity, life state, and persistence mode |
| `hud_battle_state` | `AAuraHUD` / replicated battle director | Left-top panel | Phase, event ID, active/max/pending population, and casualties |
| `hud_interaction` | Target interaction controller | Bottom panel | Relationship, life, activity, attack permission, and Interact/Trade options |
| `hud_economy` | `AAuraHUD` / owner-only PlayerState components | Right-top panel | Wallet, inventory, revisions, and registry display names |
| `hud_merchant` | `AAuraHUD` / focused merchant | Right-top panel | Merchant offers and current stock |
| `hud_merchant_result` | Purchase result delegate | Right-top panel | Server result code and revision snapshot |

Commands remain narrow and validated: `hud_interaction_select`, `hud_interaction_activate`, `hud_merchant_open`, `hud_merchant_buy`, and `hud_merchant_close`. No browser payload is trusted for role, price, inventory, stock, range, or combat permission.

## Remaining incremental polish

1. Add tutorial/first-use callouts for the Bag, Trade, and Combat protected labels once the onboarding flow is available.
2. Add an explicit save/load transition indicator if persistence exposes a replicated in-progress state; the current HUD intentionally reports only the authoritative loaded/session result.
3. Add dedicated visual QA captures for the three panels at the supported packaged resolutions and for late-join replay.

## UI test matrix

Automated coverage now checks the following contracts:

1. Each HUD page exists, loads through the native WebSocket URL placeholder, and reports readiness.
2. The left-top panel consumes role, identity, life, persistence, phase, event, active/max/pending population, and casualty fields.
3. The bottom panel consumes orthogonal target relationship/life/activity data, attack permission, and the explicit Interact route.
4. Trade opens only through the native merchant affordance and remains separate from Interact execution.
5. The right-top panel consumes owner-only wallet/inventory state, merchant stock/offers, and authoritative purchase results.
6. Native HUD delegates are bound for role, currency, inventory, and purchase-result updates.
7. Browser disconnects clear replay caches so a fresh `hud_ready` receives current state.
8. Merchant opening and purchase commands are guarded by focused native target, active merchant, Trade availability, and the native interaction component.
9. The replicated battle-director population summary accepts valid values and clamps invalid negative values.
10. All three pages parse in Node.js, mount in the transient WebUI runtime test, and retain bounded transparent geometry.

Manual acceptance cases remain necessary for visual and multi-process behavior:

- Start a new session and confirm the role name replaces “Loading role,” life leaves the default badge, and the battle summary becomes populated.
- Enter Peace → Alert/Conflict and confirm phase/event text changes without reopening the HUD.
- Focus a Civilian, observe activity changes, kill it, and confirm the target prompt clears while population pending/casualty text updates.
- Focus a merchant, select Trade, buy an affordable item, then repeat with insufficient funds and sold-out stock; verify each result is readable.
- Open Bag before and after a purchase and confirm wallet/inventory state is local to the owning player.
- Disconnect/reconnect or reload the browser panels and confirm the current state reappears without waiting for a gameplay mutation.
- Join as a second client and confirm it cannot see the first client’s wallet/inventory or merchant transaction state.
- Verify the panels remain readable at supported resolutions and blank level clicks remain native gameplay input.

## Validation

- `AuraWebUI.Plugin.RoleBattleHUDContract` asserts panel-specific role/battle/interaction/economy fields, reconnect replay, and native command gates.
- `Aura.UI.WebHUD.BattlePopulationSummary` exercises the runtime replicated summary source and fail-closed bounds.
- `git diff --check` validates the source, HTML, and documentation changes.
- JavaScript syntax checks and the AuraEditor Win64 DebugGame build are required before this increment is considered releasable.
