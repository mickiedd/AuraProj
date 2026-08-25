# In-game Web UI integration

Aura keeps its existing UMG/GAS presentation where appropriate and adds a web surface for UI that is easier to author with HTML, CSS, and JavaScript. The implementation is isolated in the `Plugins/AuraWebUI` runtime plugin; the Login and Loading levels consume its public C++ API.

## Runtime pieces

- `UWebUIWidget` is a native `UUserWidget` that hosts Unreal Engine 5.5's built-in `UWebBrowser` widget.
- `UWebUIBridgeSubsystem` is a game-world subsystem that owns the bridge lifecycle and exposes Blueprint delegates.
- `FWebUIWebSocketServer` is a small game-thread RFC 6455 text-frame server bound to `127.0.0.1` only.
- `Plugins/AuraWebUI/Content/WebUI/index.html` is a self-contained sample page. The plugin stages its web assets as non-UFS so the native file loader can read them in packaged builds.
- `Plugins/AuraWebUI/Content/WebUI/config-editor.html` is an embedded developer tool for browsing and editing the project's `Content/Config/*.json` files.

The `WebBrowserWidget` engine plugin is enabled in `Aura.uproject`. The existing BehaviorU WebSocket server remains a separate behavior-debug protocol and is not reused for gameplay UI.

## Trying the sample

In a gameplay world with `AAuraHUD`, open the Unreal console and run:

```text
AuraWebUI.Toggle
```

The plugin command creates its native widget, loads the plugin's `Content/WebUI/index.html`, and replaces `__AURA_WEBSOCKET_URL__` with the current loopback endpoint. Any Blueprint or native system can create `UWebUIWidget` without depending on Aura's HUD.

## Message contract

Browser to Unreal commands are JSON objects:

```json
{
  "type": "command",
  "command": "ping",
  "payload": { "source": "sample-page" }
}
```

Built-in commands are:

- `ready` — requests a `bridge_ready` event with port, client count, and level.
- `ping` — returns a `pong` event with a UTC timestamp.
- `get_state` — returns a `state` event with the current bridge state.

Every command is also broadcast through `UWebUIBridgeSubsystem::OnCommand`, with the command name and serialized payload JSON. Native or Blueprint gameplay code can handle domain commands there and use `SendEvent` or `SendRawJson` for responses.

## Config Studio

In a PIE or game world with a local player, open the Unreal console and run:

```text
AuraWebUI.ConfigEditor
```

For a local game launch, `-AuraConfigEditor` opens Config Studio automatically once the first local player world is ready:

```powershell
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor.exe" ".\Aura.uproject" -game /Game/Maps/StartupMap -AuraConfigEditor
```

The deferred launch hook avoids racing the map/player initialization sequence.

The editor's `Launch` dropdown also includes `Open Config Studio (PIE)`. With PIE running, it sends `AuraWebUI.ConfigEditor` to the active in-game console and opens the editor without requiring manual console input.

The command toggles the embedded Config Studio. Its native C++ service exposes only relative `.json` paths under `Content/Config`; it rejects traversal and non-JSON paths, validates submitted JSON, checks the file's content version before writing, writes through a temporary file, and preserves the previous contents as `<file>.json.bak`. The page offers a searchable file list, a structured object/array editor, and a Raw JSON tab for complex changes.

Config Studio commands use the existing bridge with request-correlated events:

- `config_list` → `config_list` with file metadata and JSON validity.
- `config_read` with `{ "path": "RoleConfig.json" }` → `config_loaded` with source text and version.
- `config_save` with `{ "path": "RoleConfig.json", "json": "...", "version": "..." }` → `config_saved` or `config_error`.

The editor is intentionally generic: it provides safe JSON authoring and does not guess domain schemas. Runtime systems remain responsible for their own gameplay-specific validation after a config is saved.

## Automated coverage

The plugin owns three automation tests under `Plugins/AuraWebUI/Source/AuraWebUI/Private/Tests`:

- `AuraWebUI.Plugin.WebSocketLoopback` starts the native loopback server, performs a browser-style HTTP upgrade, sends a masked JSON command, verifies native receipt, and verifies a JSON response frame.
- `AuraWebUI.Plugin.BridgeProtocol` creates a game world and public bridge subsystem, then verifies the built-in `ready` → `bridge_ready` and `ping` → `pong` command paths.
- `AuraWebUI.Plugin.ContentContract` verifies that the plugin descriptor and staged sample page exist and retain the WebSocket placeholder and built-in command contract.
- `AuraWebUI.Plugin.ConfigService` verifies config discovery, representative JSON loading, path traversal rejection, invalid JSON rejection, and stale-version protection without mutating project content.

The Loading level is the first migrated screen. `ALoadingPlayerController` still owns all travel, portal resolution, timeout, and progress timing; only its presentation changed from `WBP_LoadingUI` to a native `UWebUIWidget` loading `WebUI/loading.html`. Progress is sent as the `loading_progress` event, with `{ "percent": 0-100, "message": "..." }` payload data. The page sends `ready` when its browser socket opens and updates its progress bar from the event.

The Login level is also migrated. `ALoginPlayerController` still owns LevelConfig parsing, role validation, Game Server Manager queries, fallback ports, and travel to Loading. Its presentation is a native `UWebUIWidget` loading `WebUI/login.html`. The controller sends `login_state` with `{ "roles": [{ "displayName", "roleId" }], "levels": [{ "displayName", "levelId", "port" }], "selectedRoleId", "selectedLevelId", "status", "connecting" }` and `login_status` with `{ "message" }`. The page sends `login_select_role` with `{ "roleId" }`, and `login_select_level` or `login_connect` with `{ "levelId", "roleId" }`; native code validates the selected player role against `RoleConfig.json`, resolves the level ID against `LevelConfig.json`, and then invokes the existing login/travel methods. The server remains the final authority and revalidates the role from the `Role` travel option.

Run the focused suite from the project root with:

```powershell
& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ".\Aura.uproject" -unattended -nop4 -nullrhi -NoSplash -DisablePlugins=RiderLink '-ExecCmds=Automation RunTests AuraWebUI; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=.\Saved\Logs\AuraWebUIAutomation.log'
```

The test uses port `18766`; the runtime bridge default remains `18765`.

## Boundaries

- The listener is loopback-only; it is not a remote gameplay API.
- The page does not receive arbitrary UObject bindings. Commands are explicit JSON and must be handled by the bridge consumer.
- The bridge is not created for dedicated-server worlds.
- Existing UMG widgets remain available for screens that have not migrated yet; migration can happen screen by screen.
- To reuse the system in another project, copy/enable `AuraWebUI` and its `WebBrowserWidget` dependency; no Aura game-module dependency is required.
