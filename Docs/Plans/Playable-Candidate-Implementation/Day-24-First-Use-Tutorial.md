# Day 24 — First-Use and Tutorial Slice

Status: Planned  
Depends on: Day 23 boot flow

## Goal

Teach a new player the minimum movement, targeting, attack, Interact/Trade, and persistence loop without developer instructions.

## Work

- Add a small data/config-driven sequence of first-use steps and completion state.
- Reuse existing bounded WebUI panels and event contracts for callouts, target prompts, protected-target explanations, merchant affordances, and save/reconnect messaging.
- Provide a fresh-profile path and a reset/replay path without mutating authoritative gameplay state from the browser.
- Keep the tutorial optional after completion and safe for late join/reconnect.

## Detailed execution contract

### Files to inspect or create

- **New planned data:** `Content/Config/PlayableCandidateTutorial.json`, with schema version and stable step IDs.
- **Native owners:** `AuraPlayerController`, `AuraPlayerState`, `AuraInteractionComponent`, targeting/attack validation, economy/merchant components, and the persistence checkpoint boundary.
- **Presentation:** the existing WebUI callout/prompt contracts under `Plugins/AuraWebUI/Content/WebUI/` and `WebUIBridgeSubsystem`.
- **New output:** `Saved/Reports/PlayableCandidate/<Revision>/<RunId>/day-24-tutorial.json` and packaged walkthrough captures.

### Tutorial data contract

Define six ordered steps: `movement`, `target`, `attack`, `interact`, `trade`, and `save-reconnect`. Each step carries a stable ID, instruction text, server-observed completion predicate, unavailable/reason text, role support, and optional completion event. Completion belongs to the player profile; the browser can render or request a reset but cannot mark a step complete.

### Detailed steps

1. Inventory existing target, attack, Interact, Trade, save, and WebUI events and map them to the six predicates.
2. Add the versioned config and validate duplicate IDs, missing predicates, missing text, invalid ordering, and unsupported role references.
3. Initialize a fresh profile at step one without granting gameplay state from WebUI.
4. Add a server-validated reset/replay request scoped to the current owner/session; reject stale, non-owner, and disconnected requests.
5. Persist completion and replay the first incomplete step after `hud_ready`, pawn replacement, late join, and reconnect.
6. Walk Aura and BungeeMan through combat and merchant interaction, including one protected/unavailable action with an understandable reason.
7. Verify completed profiles skip mandatory prompts, reset returns to step one, and malformed tutorial data fails closed.

### Required automation and gate

- The fresh walkthrough reaches save confirmation within 10 minutes of role-ready without developer instructions.
- Native tests prove authority-owned predicates and reset; WebUI tests prove rendering/replay and no direct mutation.

## Deep-review closure

- **Owner surfaces:** planned `Content/Config/PlayableCandidateTutorial.json`, the bounded WebUI callout/prompt contracts, and the existing native interaction, combat, commerce, and persistence authorities.
- **Required artifacts:** versioned tutorial schema, fresh/reset/replay state report, six-step walkthrough (movement, target, attack, Interact, Trade, save/reconnect), and packaged captures at the Day 36 supported profiles.
- **Gate:** a fresh profile completes all six steps without developer instructions, a completed profile does not replay mandatory prompts, reset is an explicit server-validated command, and reconnect restores tutorial state without a gameplay mutation.

## Validation and evidence

- Walk a fresh profile from spawn to first combat, first Interact/Trade, and visible save confirmation.
- Verify incomplete, completed, reset, and reconnect states through native assertions and WebUI contract tests.
- Capture the flow at declared common resolutions.

## Completion gate

A player unfamiliar with AuraProj can reach combat and a merchant, understands why an action is unavailable, and can recognize that progress was saved.

## Defer

Do not build a quest system, dialogue tree, or large onboarding framework.
