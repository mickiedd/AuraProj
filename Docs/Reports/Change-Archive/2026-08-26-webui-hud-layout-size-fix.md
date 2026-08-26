# Web UI HUD layout size fix

## Intent

The post-restart PIE screenshot proved the browser and bridge were healthy, but the skill panel still rendered at zero height. The viewport slot used a bottom anchor with negative size offsets, so Slate had no drawable browser region.

## Changed behavior

- The skill panel now uses stretched vertical anchors (`0.72`–`0.98`) and zero vertical offsets, producing a real bottom-center band.
- Native health/mana UI remains untouched; native spell globes still collapse only after `skill_panel_ready` and return on disconnect.
- The static HUD contract now guards the exact non-zero-height anchor/offset shape.

## Validation

- AuraEditor Win64 Development build passed after the layout correction.
- `Aura.UI.WebSkillPanel.HUDContract` passed.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback` passed, including HTML load, bridge handler binding, and native fallback handoff.
- Fresh PIE screenshot visibly shows the brown/gold `SKILLS` HTML panel with all eight slots at the bottom of the gameplay viewport.

## Illustration

[Open the zero-height-to-visible layout diagram](2026-08-26-webui-hud-layout-size-fix.svg)
