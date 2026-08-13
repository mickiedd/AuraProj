# AuraWebUI automation tests

## Intent

Ensure the separated `AuraWebUI` plugin has executable coverage for both its packaged web content and its browser-to-native WebSocket boundary.

## Changed behavior

- Added `AuraWebUI.Plugin.WebSocketLoopback`, which uses a real loopback TCP client to perform the WebSocket upgrade, send a masked JSON command, verify native delivery, and verify the JSON response frame.
- Added `AuraWebUI.Plugin.BridgeProtocol`, which creates a game world and verifies the public bridge subsystem's `ready` and `ping` command responses.
- Added `AuraWebUI.Plugin.ContentContract`, which verifies the plugin descriptor and sample page retain the runtime dependency and message URL/command contract.
- Documented the focused Unreal automation command in `Docs/Reference/WebUI-Integration.md`.

## Validation

- `AuraEditor Win64 Development` build passed.
- `AuraServer Win64 Development` build passed.
- Focused `AuraWebUI` automation run passed all three discovered tests with exit code 0.

## Illustration

[View the AuraWebUI test coverage diagram](./2026-08-13-aurawebui-automation-tests.svg)
