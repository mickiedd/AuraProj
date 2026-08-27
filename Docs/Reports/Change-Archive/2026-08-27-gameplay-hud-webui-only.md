# Gameplay HUD WebUI-only refactor

## Intent

Complete the gameplay HUD migration started by the recent skill-panel commits. The runtime HUD must use WebUI presentation exclusively instead of instantiating the old WBP overlay or drawing native Canvas fallback text.

## Changed behavior

- `AAuraHUD::InitOverlay` now creates the existing GAS/widget-controller objects as a nonvisual data/action layer and mounts one transparent, full-screen `UWebUIWidget` loading `WebUI/hud.html`.
- `hud.html` renders vitals, experience/level, equipped skills, attributes, spell catalog/equip flow, interaction previews, messages, location readout, and quit confirmation.
- The bridge now carries controller state and explicit commands for attribute upgrades, spell selection/spending/equipping, interaction selection/activation, location toggling, and quit travel.
- The native WBP overlay is not constructed, native buttons are not broadcast, Canvas HUD text is not drawn, and browser disconnect does not restore a native HUD.
- `AAuraPlayerController::WebInteractPressed` exposes the existing interaction action to the WebUI command boundary.
- HUD, plugin content, and integration documentation now describe the complete WebUI-only gameplay path.

## Validation

- UE5.5 `AuraEditor` Mac Development build passed.
- `Aura.UI.WebSkillPanel` passed 2/2, including the full-page runtime mount/no-native-forwarding test.
- `AuraWebUI` passed 4/4, including the new `hud.html` content contract.
- `Aura.Abilities` passed 9/9.
- `hud.html` JavaScript syntax check and `git diff --check` passed.
- Rebuilt Unreal Editor launched from `/Volumes/M2/Engine/UE_5.5` with `/Volumes/M2/Works/AuraProj/Aura.uproject`.

## Illustration

[Gameplay HUD WebUI-only flow](./2026-08-27-gameplay-hud-webui-only.svg)
