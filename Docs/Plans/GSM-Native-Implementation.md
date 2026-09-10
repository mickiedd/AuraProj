# Native GSM implementation contract

Date: 2026-09-08. Target: Windows x64 standalone C++17 executable with a browser dashboard, independent of Unreal and Python at runtime.

## Existing behavior and scope

`Scripts/GameServerManager.py` owns one child per configured level, accepts newline JSON on TCP 9000, launches editor or packaged servers, and returns an endpoint only after a per-launch nonce authenticated `server_ready`. `GameServerClient.cpp` sends `request_server`; `AuraGameModeBase.cpp` sends readiness. Reuse `Content/Config/LevelConfig.json` without modifying game code or content. Existing unrelated working-tree changes are outside this job.

This release supports local Windows operation, with both listeners bound exclusively to 127.0.0.1. Keep the Python launcher intact for existing LAN deployments; add an explicit native launcher. Remote management, TLS/accounts, Linux/macOS, automatic builds, automatic adoption of unrelated processes, player counts, and persistence changes are deferred. Never silently relax a requested remote bind. No invented player/health metrics: world readiness is distinct from process liveness.

## Stage 1 — build and configuration contract

Surfaces: `Tools/GSM/CMakeLists.txt`, `Tools/GSM/src/main.cpp`, `Tools/GSM/third_party/`, `Scripts/BuildNativeGSM.ps1`, `StartNativeGSM.bat`.

1. Vendor a version-pinned MIT JSON parser with license; use Windows sockets, process handles, crypto random nonces, memory counters and kill-on-close job objects.
2. Validate configuration before listening: nonempty unique safe level IDs, map paths, unique valid game/query ports and string launch arguments. Resolve project root explicitly. Require an operator-owned server executable; editor selection may use only configured `UE_EDITOR_EXE`, never execute a path supplied by a network request.
3. Build with Visual Studio C++ tools and CMake. Fail startup with nonzero exit for malformed config, missing web assets, invalid options or occupied ports. Missing DS binary remains a visible diagnostic and a request error, permitting later build/retry.

Gate: release executable builds; invalid configuration and duplicate binds fail cleanly. Artifacts under `Saved/GSMNative/` (ignored); reproducible commands in README.

## Stage 2 — native ownership and compatible TCP protocol

Surfaces: `Tools/GSM/src/main.cpp`, `Tools/GSM/tests/`.

1. Bound all sockets and input (4096-byte protocol line, five-second input timeout, 64 concurrent connections), use a nonblocking event loop, and return explicit JSON errors.
2. Authenticate request/server tokens from existing environment variables when set. Generate a fresh 256-bit readiness nonce for each launch; pass it only in the child environment along with the actual manager endpoint and token configuration.
3. Launch with correctly quoted Unicode arguments, configured working directory, map, game/query ports, absolute log, optional persistence provider. Attach the suspended child to its own kill-on-close job before resuming it.
4. Coalesce concurrent requests for the same level. State is stopped → starting → ready → exited/failed. Only matching live process, port and nonce can mark ready. Configured public host owns routing. Reject incompatible runtime requests instead of disrupting an active server.
5. Enforce one 30-second launch deadline shared by waiters; on expiry terminate the owned process tree and fail every waiter. Record PID, exit code, launch count and failure. Shutdown closes owned jobs; never kill processes by name or port.

Gate: real child-process integration fixtures prove readiness, no premature ready, wrong nonce/port/token rejection, concurrency coalescing, timeout cleanup, process exit, recovery, malformed input and manager shutdown cleanup. Fixture is a compiled C++ fake DS; Python is only the test driver.

## Stage 3 — web dashboard

Surfaces: `Tools/GSM/web/index.html`, `app.js`, `style.css`, snapshot API in C++.

1. Serve an allowlisted set of static files and `GET /api/status`; no arbitrary filesystem routes or mutation API. Check HTTP Host against the actual loopback endpoint to resist DNS rebinding; do not enable CORS.
2. Publish configured/managed-running/ready/starting/failed counts, manager uptime and endpoints, per-level map, ports, PID, executable, redacted argv, log path, elapsed uptime, working-set/private memory, CPU time, exit status and launch error. Counts explicitly cover only GSM-owned processes.
3. Poll every two seconds with bounded fetch time, show stale/offline state and last successful refresh, support search and selected process details, render data with text nodes, and preserve selection between polls. Never expose tokens, launch nonces or raw environment.

Gate: API assertions, secret-redaction fixture, malformed/oversize HTTP and path rejection, desktop and narrow viewport visual checks, backend-offline state.

## Stage 4 — delivery and validation

Surfaces: `Tools/GSM/README.md`, `Docs/Reports/Change-Archive/2026-09-08-native-gsm-dashboard.{md,svg}`, `.claude/memory/visual-change-archive.md`; packet outside repository.

1. Run build and native integration suite; run existing Python GSM regression tests to ensure the retained fallback remains valid.
2. Open the real dashboard and inspect its rendered states. Attempt a real Unreal DS readiness smoke test when a compatible binary is available; distinguish fixture success from gameplay validation.
3. Review implementation against these contracts and record any unavailable validation; prepare the local implementation validation packet using the current handoff skill, including all changed files, commands, logs and review questions. Packet preparation is not independent review.
4. Create and inspect a before/after SVG, matching archive record and append-only index entry. Preserve prior records and unrelated changes.

Completion gate: implementation, tests, dashboard visual evidence, operational instructions and archive exist. Explicitly report unrun gameplay/independent validation rather than claiming those passed.
