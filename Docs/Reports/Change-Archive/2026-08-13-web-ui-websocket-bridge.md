# In-game Web UI and WebSocket bridge

Date: 2026-08-13

## Intent

Add an in-game HTML/CSS/JavaScript authoring path without removing the existing UMG/GAS UI architecture.

## Changed behavior

- Enabled Unreal Engine 5.5's built-in `WebBrowserWidget` plugin.
- Added `UWebUIWidget`, which hosts the native `UWebBrowser` UMG control and loads the plugin-owned `Content/WebUI/index.html`.
- Added a game-world `UWebUIBridgeSubsystem` with a loopback-only WebSocket server on port `18765`.
- Added explicit JSON `ready`, `ping`, and `get_state` messages plus Blueprint/native `OnCommand`, `OnConnectionChanged`, `SendEvent`, and `SendRawJson` hooks.
- Added the standalone `AuraWebUI` runtime plugin and its `AuraWebUI.Toggle` command; existing Aura UMG/HUD source is unchanged.
- Staged the plugin's web assets as non-UFS and documented the reusable plugin contract in [WebUI-Integration.md](../../Reference/WebUI-Integration.md).

## Important guard

The listener binds to `127.0.0.1` only and is not a remote gameplay API. It is also not created for dedicated-server worlds. The existing BehaviorU debug WebSocket server remains isolated because it has a separate behavior-tree protocol.

## Validation

- `AuraEditor Win64 Development` build passed.
- `AuraServer Win64 Development` build passed.
- HTML structural checks passed for the WebSocket client and replacement placeholder.
- Existing project warnings remain: AuraAbilityGraph dependency/circularity warnings and pre-existing GAS deprecation warnings.

## Illustration

[Open the before/after architecture diagram](2026-08-13-web-ui-websocket-bridge.svg)
