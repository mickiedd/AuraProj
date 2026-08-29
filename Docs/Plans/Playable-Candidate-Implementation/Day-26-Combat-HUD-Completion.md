# Day 26 — Combat HUD Completion

Status: Planned  
Depends on: Day 24 tutorial and Day 25 ammo state

## Goal

Make combat state readable: the player must know whether a shot can fire, what it targeted, what happened, and when recovery is required.

## Work

- Extend the bounded WebUI HUD with magazine/reserve ammo, reload state, target validity, attack permission, hit/kill feedback, and life-state messaging.
- Reuse native replicated state and narrow command events; no browser payload becomes authoritative.
- Ensure HUD state replays after `hud_ready`, browser reconnect, pawn replacement, late join, and death/recovery.
- Preserve native world input through uncovered viewport regions.

## Detailed execution contract

### Files to inspect or modify

- **WebUI pages:** `Plugins/AuraWebUI/Content/WebUI/hud-left-top.html`, `hud-right-top.html`, `hud-bottom.html`, `index.html`, and the existing login/loading pages.
- **Bridge/runtime:** `Plugins/AuraWebUI/Source/AuraWebUI/Public/UI/WebUI/WebUIBridgeSubsystem.h/.cpp`, `WebUIWidget.h/.cpp`, `Source/Aura/Public/UI/HUD/AuraHUD.h/.cpp`, `AuraPlayerController`, and the replicated PlayerState/effect event sources.
- **Tests/output:** `Plugins/AuraWebUI/Source/AuraWebUI/Private/Tests/AuraWebUIAutomationTests.cpp`, `Source/Aura/Private/Tests/AuraRoleBattleDay26Tests.cpp`, `RunPlayableCandidateDay26HUD.ps1`, and `day-26-hud.json`.

### HUD state contract

The bridge exposes versioned presentation state with `RoleId`, `LifeState`, target descriptor/validity, attack availability/reason, a role-aware firearm state (`MagazineRounds`, `ReserveRounds`, reload status for BungeeMan, or explicit `NotApplicable` for Aura), last accepted attack result, merchant focus/availability, and owner-scoped wallet/inventory revisions. Events are replayable snapshots, not authority commands. A `hud_ready` request asks for a snapshot; it does not create gameplay state.

### Detailed steps

1. Inventory the existing three panel message names and native delegates; map each to a single state field and an owner/visibility rule.
2. Add schema/version validation and explicit empty/unknown defaults so a stale browser cannot display a prior player's values.
3. Render ammo/reload, target/attack reason, hit/kill, life/death/recovery, merchant result, and save/reconnect feedback using the existing panels.
4. Implement one idempotent snapshot replay on WebUI ready, browser reconnect, pawn replacement, late join, and recovery; clear state on disconnect before the next identity is bound.
5. Preserve native input: verify transparent/uncovered regions, focus changes, pointer capture, and keyboard/mouse gameplay controls around every panel.
6. Exercise Aura and BungeeMan, owner and non-owner clients, normal/empty/reloading/invalid-target/hit/death/reconnect states, and stale-event ordering. Empty/reloading firearm states are exercised for BungeeMan; Aura must render the explicit firearm `NotApplicable` state and still exercise the common attack/result states.
7. Capture packaged screenshots at the Day 36 profiles and record the panel/input checklist in the JSON artifact.

### Named automation and commands

- WebUI tests cover state schema, event replay, malformed payload rejection, reconnect reset, and input hit-testing; native tests cover authority-to-bridge mapping.
- Run the WebUI contract suite, focused Role/Battle tests, `RunPlayableCandidateDay26HUD.ps1 -Mode Listen`, and `-Mode Dedicated`.
- A failure is any missing state, duplicate replay, stale identity, non-owner field, clipped/blocked panel, or runner nonzero exit; the artifact records the exact event sequence.

## Validation and evidence

- Add WebUI contract tests for normal, empty, reloading, invalid-target, hit, death, and reconnect states.
- Capture the HUD in a packaged build at supported resolutions and verify input hit-testing around every panel.
- Run Aura and BungeeMan through the same panel contract.

## Deep-review closure

- **Owner surfaces:** `Plugins/AuraWebUI/Content/WebUI/hud-left-top.html`, `hud-right-top.html`, `hud-bottom.html`, `WebUIBridgeSubsystem`, and the native replicated state/event boundary. No HUD command may write authoritative combat or economy state.
- **Required artifacts:** versioned HUD state contract, `day-26-hud.json`, packaged captures for both roles, and an input hit-test checklist for each supported visual profile.
- **Gate:** normal, invalid-target, hit, death, late-join, and reconnect states render for both roles; BungeeMan additionally renders normal, empty, and reloading firearm states while Aura renders firearm `NotApplicable`; `hud_ready` and pawn replacement replay the current authoritative state once; no panel clips, blocks world input, or reveals another player's private ammo/economy state.

## Completion gate

Players can explain why a shot is unavailable and can see the authoritative result of an accepted attack without reading logs.

## Defer

Do not add a second HUD technology or a screenshot-diff platform.
