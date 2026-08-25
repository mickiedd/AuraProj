# Startup ability HUD replay

## Intent

Restore startup skill icons when ability replication completes before the overlay and its spell-globe listeners finish binding.

## Changed behavior

- Deferred the controller's startup ability publication out of dependency binding.
- Added ability replay to `UOverlayWidgetController::BroadcastInitialValues`, which `AAuraHUD` calls after assigning the controller to the overlay widgets.
- Preserved `AbilitiesGivenDelegate` handling for the opposite ordering, where startup abilities arrive after the HUD is ready.
- Added a dynamic-delegate regression fixture and an executable automation test that recreates the observed early-replication ordering with Fire Bolt, Fire Blast, Arcane Shards, and Electrocute in `LMB`, `1`, `2`, and `3`.

## Validation

- `AuraEditor` Win64 DebugGame build passed, including UnrealHeaderTool, the changed controller, the UHT-visible delegate receiver, and the regression test.
- `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding` passed and observed all four broadcasts with their correct input slots and equipped status.
- All 9 `Aura.Abilities` automation tests passed.
- The active Sci-Fi dedicated server remained healthy and listening on UDP 7784; its log retains the successful `mickie` join. The HUD change is client presentation only and does not alter server grant or replication behavior.
- The Win64 Development build compiled the changed sources; its final DLL link was unavailable because two running Unreal Editor processes held `UnrealEditor-Aura.dll`. The independent DebugGame target provided a complete link and executable validation without interrupting the active editor/server session.
- `git diff --check` and SVG XML validation passed.

Illustration: [Startup ability HUD replay](2026-08-25-startup-ability-hud-replay.svg)
