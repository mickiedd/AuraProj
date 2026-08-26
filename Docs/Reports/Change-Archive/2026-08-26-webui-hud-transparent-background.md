# Web UI HUD transparent browser background

## Intent

Remove the opaque black surface that surrounded the in-game Web UI skill panel even though the page itself declared a transparent body.

## Changed behavior

- `UWebUIWidget` now enables `UWebBrowser`'s protected `bSupportsTransparency` property through reflection before Slate creates the native browser surface.
- CEF/Slate therefore alpha-composites the HTML page over the game viewport; the gold/brown skill panel remains visible while the area outside it shows the game world.
- The widget exposes a runtime diagnostic, and automation verifies both the native setting and the mounted page.

## Validation

- AuraEditor Win64 Development build passed after the transparency change.
- `AuraWebUI.Plugin.ContentContract` passed, including the source guard for the native transparency setup.
- `Aura.UI.WebSkillPanel.HUDContract` passed.
- `Aura.UI.WebSkillPanel.RuntimeMountAndFallback` passed, including the runtime transparency assertion and 8,841-byte skill page load.
- Fresh PIE screenshot: [transparent skill HUD](../../../Saved/Logs/Transparency-pie-clean.png). The game floor is visible around and through the panel host; no black outer rectangle remains.

## Illustration

[Open the opaque-host-to-alpha-composited HUD diagram](2026-08-26-webui-hud-transparent-background.svg)
