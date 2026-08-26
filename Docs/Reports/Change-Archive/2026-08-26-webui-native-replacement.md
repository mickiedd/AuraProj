# Web UI native skill replacement

## Intent

Make the HTML skill panel the visible skill-bar implementation instead of leaving the old `WBP_HealthManaSpells` spell globes on screen beside it.

## Changed behavior

- The skill panel is mounted at z-order 200 and explicitly reloads `WebUI/skill-panel.html` after mounting, so the configured page cannot be replaced by the generic Web UI default during construction.
- When the page sends `skill_panel_ready`, `AAuraHUD` walks the native overlay tree and collapses only widgets whose name/class contains `SpellGlobe`. Native health and mana widgets remain available.
- If the browser disconnects, the native spell globes are restored as a safe fallback.
- Runtime logging now records the mounted HTML path, payload size, and the native-globe handoff state.

## Validation

- AuraEditor Win64 DebugGame build passed.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback` passed: mounted the real `skill-panel.html` (8,841 bytes), created the native browser root, verified the command handler binding, collapsed the LMB globe on readiness, and restored it on disconnect. Headless mode has no viewport, so the test records that limitation while still executing the live widget/HTML path.
- `Aura.UI.WebSkillPanel.HUDContract` passed with the mount/reload and readiness replacement checks.
- `AuraWebUI` automation suite passed 4/4.
- No delegate-binding ensure signatures appeared in the new automation logs.
- The active Development editor must be restarted or Live Coding compiled before an existing PIE session loads this change.

## Illustration

[Open the native-to-Web UI handoff diagram](2026-08-26-webui-native-replacement.svg)
