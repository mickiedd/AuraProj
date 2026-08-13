# Refactor Web UI into the AuraWebUI plugin

Date: 2026-08-13

## Intent

Separate the in-game Web UI and WebSocket bridge from the Aura game module so the system can be enabled and reused as an independent Unreal runtime plugin.

## Changed behavior

- Added `Plugins/AuraWebUI/AuraWebUI.uplugin` with the `AuraWebUI` runtime module and a dependency on Unreal's `WebBrowserWidget` plugin.
- Moved `UWebUIWidget`, `UWebUIBridgeSubsystem`, the WebSocket transport, and the sample HTML into the plugin.
- Changed asset loading from `ProjectContentDir()` to the plugin base directory and staged plugin web assets as non-UFS.
- Removed Web UI dependencies and APIs from `Source/Aura`: no `Aura.Build.cs` browser dependency, no `AAuraHUD` web widget state, and no Aura primary-module console command.
- Added the plugin-owned `AuraWebUI.Toggle` command as a convenience entry point.
- Updated the integration guide to describe plugin reuse and retained the original bridge archive as immutable history.

## Boundary

Aura only enables `AuraWebUI` in `Aura.uproject`. The plugin's public classes use `AURAWEBUI_API` and do not include Aura headers or depend on the Aura module.

## Validation

- `AuraEditor Win64 Development` build passed with the plugin module compiling and linking.
- `AuraServer Win64 Development` build passed; the bridge subsystem remains disabled for dedicated-server worlds.
- Static ownership check found Web UI source only under `Plugins/AuraWebUI` and the project plugin enablement entry.

## Illustration

[Open the plugin refactor diagram](2026-08-13-aurawebui-plugin-refactor.svg)
