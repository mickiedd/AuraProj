# GSM recovery and Scifi Desert PIE connection

## Intent

Repair the client path that stalled while connecting to a dedicated server, address the GSM duplicate-port failure, and prove the Scifi Desert Level login inside the Unreal Editor Play In Editor (PIE) window.

## Changed behavior

- `Scripts/GameServerManager.py` now treats `127.0.0.1:9000` `EADDRINUSE` as a clean duplicate-manager exit instead of an uncaught traceback.
- A startup or `server_ready` deadline now clears the level's ready state and stops the unready dedicated-server child, preventing stale processes from poisoning retries.
- The active `AuraManifest_B9154B90_B.sav` was recovered from the preserved candidate and backup set. The first cold restart loaded the map-correct generation 188 checkpoint; the successful PIE session then advanced the stable active checkpoint to generation 192 and loaded `L_showcase_level` with 32 live civilians.

## Validation

- `python -m py_compile Scripts\GameServerManager.py`
- `python Scripts\test_gsm_request_deadline.py` — 5 tests passed, including timeout cleanup and port-conflict handling.
- `python Scripts\test_game_server_manager.py` — passed.
- `git diff --check` — passed.
- A second GSM launch while the live manager owned port 9000 exited with the expected “address is already in use” error and no traceback.
- Fresh PIE run: Login selected **Scifi Desert Level (7784)**; GSM returned `127.0.0.1:7784` after 15.25 seconds; server logged `WorldReadiness State=Ready`, `PreLogin Accepted`, and `Join succeeded: mickie`; client logged `Welcomed by server` and `TravelCompleted` for `/Game/Scifi_desert_city/Level/L_showcase_level`.

## Visual evidence

- [Change diagram](2026-09-08-gsm-recovery-and-cleanup.svg)
- [Fresh PIE gameplay capture](../../../Saved/Reports/PIEConnectionRepair-20260907/scifi-pie-gameplay-20260908-fresh.png)

The remaining AI navigation, marker-root, missing Crunch asset, and empty engine-version warnings are non-blocking runtime warnings; they did not prevent readiness or login.
