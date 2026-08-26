# Web UI HUD viewport context

## Intent

The runtime screenshot showed the native spell globes hidden while the HTML replacement was not visible. Align the skill-panel widget's ownership with the existing login/loading WebUI widgets without breaking headless automation.

## Changed behavior

- Real gameplay creates the skill-panel widget from the local `APlayerController`, so it is attached to the active player viewport layer.
- Headless automation controllers without a `ULocalPlayer` continue to use the world context.
- The top z-order, explicit page reload, native-globe readiness handoff, and health/mana fallback remain unchanged.

## Validation

- AuraEditor Win64 DebugGame build passed; only the pre-existing `FImageUtils::CompressImageArray` deprecation warning remains.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback` passed after the ownership change: real page loaded (8,841 bytes), bridge listener bound, native spell globes collapsed/restored.
- HUD contract coverage now asserts both viewport-context branches.

## Illustration

[Open the viewport ownership guard diagram](2026-08-26-webui-hud-viewport-context.svg)
