# Aura native Game Server Manager

A Windows x64 C++17 application that owns dedicated server processes and displays its dashboard inside a native WebView2 window. Python is not used by the application; it is used only by test drivers. The loopback HTTP dashboard remains available for optional browser access.

## Build and run

Build prerequisites: Visual Studio 2019 or 2022 with Desktop development with C++ and CMake, Windows SDK. No npm or network downloads are needed to build: nlohmann/json 3.11.3 and Microsoft WebView2 SDK 1.0.2651.64 are vendored with their licenses. Desktop prerequisite: [Microsoft Edge WebView2 Evergreen Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/). The loader is statically linked; no WebView2Loader.dll needs to be deployed.

From the project root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/BuildNativeGSM.ps1
.\StartNativeGSM.bat
```

The **Aura GSM | Dedicated Server Manager** window opens automatically with its dashboard. No separate browser step is needed. Build output is `Saved/GSMNative/build/Release/AuraGSM.exe` with a `web` directory beside it. `StartNativeGSM.bat` builds on first launch; rerun the build script after source or frontend changes. Running the executable requires the VC++ runtime. Closing the window stops GSM and terminates its owned process trees; this is forced cleanup, not a gameplay save-and-drain operation. Minimize keeps GSM running.

Use `.\StartNativeGSM.bat --headless` for backend-only operation without WebView2 initialization. Headless mode supports redirected logs and Ctrl+C. Browser access remains at http://127.0.0.1:9080. GUI errors appear in a native dialog; headless errors go to stderr and return nonzero.

The WebView profile is stored in `Saved/GSMNative/WebView2/<web-port>/` under the project root. The window resizes its view and handles DPI changes. Initialization has a 30-second timeout. A missing/broken runtime, initial navigation failure or browser-process failure stops desktop GSM and its owned processes with an error; `--headless` is an explicit alternative. The embedded view permits only its own dashboard document, blocks popups and permission requests, and exposes no native JavaScript bridge.

**Cutover:** stop the existing Python GSM through its console before launching native GSM on the same TCP port. Do not run two managers against the same game ports. This job leaves the running Python manager and `StartGameServer.bat` unchanged. Native does not adopt existing Python-owned or manually started servers; its counts explicitly cover only its own child processes.

For an inspection-only preview alongside Python:

```powershell
.\StartNativeGSM.bat --port 19000 --web-port 19080
```

The native window automatically uses port 19080. This preview reads real level definitions but cannot launch real Unreal servers on a different GSM port: native requires that port to match `Content/Config/ServerConnection.json`. Normal native operation must use the `gameServerPort` in that file (currently 9000). No configuration files are rewritten by GSM.

## Executable and environment configuration

`--server-exe PATH` or `AURA_SERVER_EXE` is an operator-owned explicit override. Without it, native checks `Saved/StagedBuilds/WindowsServer/AuraServer.exe` for a cooked payload, then falls back to configured `UE_EDITOR_EXE`. It deliberately does not automatically launch the uncooked `Binaries/Win64/AuraServer.exe`. Archived packages can be selected explicitly with `--server-exe`.

Editor clients need no environment configuration. The editor menu supplies its current executable automatically. Standalone launches discover the project's `EngineAssociation` through Unreal's per-user build registration, installed-engine registration, or a sibling `UE_<association>` directory, then select the client's exact editor variant. Trailing separators on reported engine paths are accepted. A stale `UE_EDITOR_EXE` cannot override successful discovery; it remains an optional legacy fallback, and `--editor-exe` is an explicit override. Unknown engines or unavailable variants produce an actionable error directing the user to launch from the editor menu. Client-provided paths are never used as execution authority. An active level with a different runtime is rejected; restart the manager to switch runtimes. Native does not invoke Unreal builds.

Options: `--project-root`, `--config`, `--web-root`, `--server-exe`, `--editor-exe`, `--port`, `--web-port`, `--public-host`, `--persistence-provider`, `--host 127.0.0.1`, `--headless`, `--help`. Paths resolve from the invocation directory. Defaults: current directory for project root when invoking the executable directly; TCP port from LevelConfig (or 9000); HTTP 9080; public host 127.0.0.1; persistence provider NULL. The root launcher always supplies the actual project root.

Supported environment: `AURA_SERVER_EXE`, `UE_EDITOR_EXE`, `AURA_GSM_PORT`, `AURA_PUBLIC_HOST`, `AURA_PERSISTENCE_PROVIDER`, `AURA_GSM_AUTH_TOKEN`, `AURA_GSM_SERVER_AUTH_TOKEN`. The server token falls back to the client token. Existing non-loopback `AURA_GSM_HOST` is rejected. Remote binds, CIDR allowlists and web authentication are deferred: both listeners are strictly local. Local OS users can read the dashboard; do not use it as a multi-user security boundary.

Child arguments include map URL, configured launch arguments, game port, optional query port, absolute log path, and persistence provider. Each child gets its own environment nonce, manager address and server token. The nonce never appears in argv, logs or the status API. Sensitive argument keys (token/password/secret/nonce) and configured auth token values are redacted in the dashboard. Do not place credentials in level names, map paths or arbitrary unlabeled arguments.

## Data and failure contracts

- `GET /api/status`: version, manager PID/uptime, TCP/web endpoints, advertised host, request count, configured/running/ready/starting/failed totals, and per-level records. Snapshot schema is represented by `Manager::snapshot` in `src/main.cpp`.
- Per-level: ID/name/map, game/query ports, state, last PID/executable/argv/log path, run duration, launch count, exit code/error, working-set and private bytes, total CPU seconds. Unavailable metrics are JSON null. CPU is cumulative time, not utilization percentage. Runtime args are from the last launch; stopped levels expose configured args.
- State flow: stopped → starting → ready; process exit → exited; launch timeout/failure → failed. Readiness requires a live owned process, matching level/port and per-launch nonce. Ready is a world-startup callback, not a heartbeat or guarantee of gameplay health.
- Each level has a shared 30-second launch deadline. Concurrent requests coalesce. On expiry, close the ownership job and fail waiters. A retry creates a new process and nonce. Process exits invalidate readiness and preserve exit code. No automatic restart loop.
- TCP uses one newline-delimited JSON message per connection, compatible with `request_server` and `server_ready`. It validates token fields when configured and returns the configured host/port, never the callback's host.
- Limits: 4096 bytes per TCP request, 8192-byte HTTP headers, five-second incomplete-request/write deadlines, 64 admitted connections and at most 48 readiness waiters to leave room for callbacks. Excess connections close; excess waiters receive an explicit retry error. Disconnected waiters may occupy their slot until launch resolves.
- HTTP serves only `/`, `/index.html`, `/app.js`, `/style.css`, `/api/status`; no file traversal or mutation endpoints. Loopback Host validation, no CORS, no-store responses and CSP protect local browser access. The dashboard polls every two seconds, marks retained data stale on failure and retries automatically.
- Player count, continuous health, machine-wide DS discovery, remote fleet control, log content viewing and start/stop buttons are not supplied in this release.

## Verification

```powershell
python Tools/GSM/tests/integration.py
python Scripts/test_game_server_manager.py
python Scripts/test_gsm_request_deadline.py
git diff --check
```

Desktop lifecycle check: run `python -B Tools/GSM/tests/desktop_fixture.py start`, inspect the native window's live fixture metrics, close that window, then run `python -B Tools/GSM/tests/desktop_fixture.py verify-closed`. This uses isolated ports 19100/19180/19190 and a compiled fixture, not a real Unreal server.

The native suite launches the compiled `GSMFixture.exe` in temporary project roots with isolated ports. It covers actual process creation/cleanup, argument quoting, nonce rotation, coalescing, memory metrics, auth and endpoint guards, timeout, exit, bad inputs and bind/config errors. It does not prove Unreal gameplay or multiplayer joins. Visual checks cover the real dashboard, responsive layout, selection/search, and offline recovery. See the dated change archive and the external implementation validation packet for this run's evidence and limitations.

Dependency provenance: https://github.com/nlohmann/json/tree/v3.11.3; header SHA256 `9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6`. Windows process ownership follows [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects) and Unicode launch behavior follows [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
