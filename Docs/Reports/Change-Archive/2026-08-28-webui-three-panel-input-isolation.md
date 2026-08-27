# WebUI three-panel HUD and level input isolation

## Intent

Separate the gameplay WebUI HUD into bottom, left-top, and right-top browser surfaces so transparent HUD interaction does not place a full-screen CEF/Slate mouse surface over the level.

## Changed behavior

- `AAuraHUD` now mounts three independent `UWebUIWidget` instances with bounded viewport slots and dedicated HTML pages.
- `hud-left-top.html` owns progress and transient messages; `hud-right-top.html` owns location, menus, and quit flow; `hud-bottom.html` owns vitals, skill buttons, and the interaction prompt.
- Blank-page pointer events no longer synthesize gameplay LMB input. Explicit skill buttons still send validated press/hold/release commands, while menu and interaction surfaces expand only while intentionally active.

## Validation

- AuraEditor Win64 Development build passed.
- `AuraWebUI` content/bridge automation passed 4/4.
- `Aura.UI.WebSkillPanel.HUDContract` and `RuntimeMountAndFallback` passed 2/2; runtime loaded all three HTML assets and created three native browser roots.
- `Aura.Abilities` passed 9/9.
- Inline JavaScript syntax checks passed for all three panel pages; `git diff --check` passed.

## Illustration

[Open the three-panel input-isolation diagram](2026-08-28-webui-three-panel-input-isolation.svg)
