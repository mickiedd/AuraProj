# Day 56 — Gameplay HUD and Onboarding

Status: Planned; not implemented by this planning job.  
Depends on: Day 55 full gameplay loop; per-feature HUD already exists from earlier days.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

A new player can understand objectives, threats, build choices, supplies and recovery without developer narration.

## Exact change surfaces

- Existing: `Source/Aura/Private/UI/HUD/AuraHUD.cpp`, `Private/UI/WidgetController/OverlayWidgetController.cpp`, `Private/Player/AuraPlayerController.cpp`.
- New: `Source/Aura/Public/UI/WidgetController/MissionWidgetController.h` and implementation; `Content/Config/GameplayGuidanceDefinitions.json`.
- Extend Day 53's versioned `GameplayGuidanceMask` in `Public/Game/AuraPlayerSaveGame.h`, save validation/checksum/migration and the existing terminal/forfeit/graceful-shutdown batch merge; preserve legacy tutorial bits.
- Existing WebUI: `Plugins/AuraWebUI/Content/WebUI/hud-bottom.html`, `hud-left-top.html`, `hud-right-top.html`, `Plugins/AuraWebUI/Source/AuraWebUI/Private/UI/WebUI/WebUIBridgeSubsystem.cpp`.

## Data and authority contract

Native view model consumes public mission and owner-private run snapshots by revision. WebUI renders and sends narrow intent only. Objective text includes action/location/progress; cues use shape and text/icon as well as color; subtitles mirror critical audio. Expose keyboard focus and mapped input names, readable text scaling, reduced camera shake and hold/toggle channel preference. Pings are server-validated tactical marks, max2/s burst3, TTL5s; no arbitrary chat text or raw identity. Guidance observes accepted actions, never client claims of tutorial completion. Its versioned gameplay mask is separate from legacy tutorial progress: suppress prompts immediately from accepted run memory, retain that memory across same-server reconnect, and merge it only through Day 53's allowed normal-terminal/member-forfeit/expiry/graceful-shutdown transactions. Forced crash restores the last committed mask; uncommitted prompts may therefore recur. Only an explicit validated hub transaction resets committed gameplay guidance.

## Numbered implementation steps

1. Consolidate objective/boss/tactical/choice/supply/recovery/result information into the existing panels and remove duplicate/conflicting counters. Preserve BungeeMan ammo/reload and Aura NotApplicable.
2. Bind state revisions and server deadlines; reconnect or stale response cannot replace newer state. Loading/no connection/timeout are visible states, not zero values.
3. Add contextual first-use prompts for evade, expose combo, objective channel, augment choice, rescue and resupply. Map stable guidance IDs to versioned mask bits, suppress after accepted server evidence and route durable merges through Day 53 without touching legacy tutorial bits or frozen inventory/attributes. Implement an explicit hub-only reset; distinguish forced-crash loss of uncommitted observations from an intentional reset in the report.
4. Add ping action through the owner command boundary and render shared marker plus role label; no client world coordinates accepted without range/LOS validation.
5. Verify keyboard-only flow, focus return after choice, rebinding labels, text scale 100/125/150%, reduced motion, high contrast and non-color threat discrimination.
6. Test actual packaged WebUI-transformed HTML at 1280×720, 1920×1080 and 2560×1440; use in-game captures for clipping/overlap/reticle obstruction and missing render assets.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay56Tests.cpp`; namespace `Aura.Gameplay.Day56`.

- SnapshotRevisionMonotonic — old RPC response cannot revert replicated mission state.
- OwnerDataNotBroadcast — another client cannot see offers, supplies ledger or wallet through WebUI payload.
- GuidanceRequiresAcceptedAction — rejected input does not complete guidance.
- GuidanceSurvivesAllowedCommits — accepted bits survive normal terminal/forfeit/expiry/graceful shutdown and reconnect; forced crash retains only the last committed mask, and hub reset changes no legacy tutorial bit.
- PingRateAndExpiry — burst/replay spam cannot exceed active marker budget or extend TTL.
- KeyboardFocusReturns — closing choice/result restores gameplay input without stuck firing/reload.
- HUDNoClipping — transformed HTML and rendered game show all required controls at three resolutions/scales.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 56 -Stage Fast -RunId d56-fast
./RunGameplayExpansion.ps1 -Day 56 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d56-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

HUDJourney: all nine functional lanes plus rendered resolution/scale cross-product in S-B and L-AB; long localized-placeholder labels, missing icon, pending request, stale snapshot, disconnect and rapid panel toggling. Guidance fixtures cover normal terminal, member forfeit/expiry, graceful shutdown, forced crash before/after commit, previous save migration and explicit hub reset.

## Failure and timeout semantics

Missing required HUD binding/asset fails content/visual gate. At two-second command timeout display Retry with same RequestId, never success. Guidance timeout must not block gameplay. Browser fixture PASS cannot replace in-game visual evidence. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-56.json, payload-privacy-report.json, focus-keyboard-results.json, resolution-scale-matrix.json, annotated in-game captures and first-use walkthrough. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All core actions are understandable and usable from keyboard at required sizes; no state/privacy regressions or obscured combat cues. Complete prompt flow is observed without manual state injection; accepted guidance persists only through the declared merge path, with legacy tutorial state unchanged.

## Defer / anti-goals

No full localization production, chat/social system, minimap overhaul, new HUD framework or JavaScript gameplay simulation.
