# In-game Web UI Skill HUD

## Intent

Move the overflowing bottom skill strip into the existing embedded Web UI layer while retaining the native HUD as a fallback.

## Changed behavior

- `UWebUIWidget` now applies responsive viewport anchors, offsets, and alignment through `UGameViewportSubsystem`.
- `skill-panel.html` renders a bounded, transparent-outside, responsive skill strip with slot state, icon data URLs, fallback badges, and pointer press/hold/release behavior.
- `AAuraHUD` creates the panel only for the local controller, replays ability state when the browser announces readiness, and sends updates through the existing loopback bridge.
- Web commands are validated against known gameplay input tags and forwarded through `AAuraPlayerController`, preserving targeting and GAS input gates.
- Native health, mana, menus, and skill widgets remain present as a fallback if the browser or bridge is unavailable.

## Tests and validation

- AuraEditor Win64 DebugGame build passed.
- `AuraWebUI` automation suite passed 4/4, including bridge protocol and the new skill-panel content contract.
- `Aura.UI.WebSkillPanel.HUDContract` passed.
- `Aura.UI.Overlay.StartupAbilitiesReplayedAfterWidgetBinding` passed.
- `Aura.Abilities` suite passed.
- The active editor was not restarted; the Development DLL therefore requires an editor restart or Live Coding compile before an existing session can display the new panel.

## Illustration

[Open the visual flow diagram](2026-08-26-in-game-webui-skill-hud.svg)
