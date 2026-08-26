# Web UI vitals and action HUD

## Intent

Move the in-game health bar, mana bar, and the Attributes, Spells, and Close controls into the bounded Web UI HUD while keeping the existing native overlay behavior available as a safe fallback.

## Changed behavior

- `skill-panel.html` now renders health and mana orbs, numeric values, ratio fills, and the three action buttons alongside the skill slots. Its page background remains transparent, so only the intentional HUD cards paint over the game.
- `AAuraHUD` forwards `OnHealthChanged`, `OnMaxHealthChanged`, `OnManaChanged`, and `OnMaxManaChanged` as `hud_vitals` events. Web button commands are resolved to the existing nested WBP buttons and invoke their `OnClicked` delegates, preserving native menu and close behavior.
- Native health/mana widgets and action buttons are collapsed only after `skill_panel_ready`; they are restored when the browser disconnects. The mount disables construct-time auto reload and performs one post-mount load, preventing duplicate WebSocket pages during PIE travel.

## Validation

- AuraEditor Win64 Development build passed after the final mount-lifecycle change.
- `Aura.UI.WebSkillPanel` passed both `HUDContract` and `RuntimeMountAndFallback`; the runtime test verified one HTML load, five native vital/action widgets hidden and restored, and all three native button delegates forwarded.
- `AuraWebUI.Plugin.ContentContract` passed for the HTML IDs/events, commands, responsive layout, transparency, and browser configuration.
- Extracted HUD JavaScript passed `node --check`; `git diff --check` passed.
- Fresh PIE screenshot: [Web UI vitals/action HUD](../../../Saved/Logs/WebUIVitals-final-clean.png).
- PIE click log confirms one Attributes, one Spells, and one Close command forwarded to `AttributeMenuButton`, `SpellMenuButton`, and `Button_Quit`; client log contains no `Ensure condition failed`, `Unable to bind delegate`, or fatal signatures. The server listen log reports `InitListen ... result=true` on port 7791.

## Illustration

[Open the vitals/action HUD data-flow diagram](2026-08-27-webui-vitals-action-hud.svg)
